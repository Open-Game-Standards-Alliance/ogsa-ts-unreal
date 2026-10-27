// TelemetryPublisherComponent.cpp — see header for the API contract.
#include "TelemetryPublisherComponent.h"
#include "Sockets.h"
#include "SocketSubsystem.h"
#include "Misc/App.h"
#include "HAL/PlatformTime.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Components/SceneComponent.h"

using namespace ogsa::telemetry;

UTelemetryPublisherComponent::UTelemetryPublisherComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    // C-11: keep streaming while the game is paused
    PrimaryComponentTick.bTickEvenWhenPaused = true;
}

void UTelemetryPublisherComponent::BeginPlay()
{
    Super::BeginPlay();
    PauseChannel = DeclareBooleanChannel(TEXT("session.paused"));

    FDateTime Now = FDateTime::UtcNow();
    SessionStartUnixUs =
        (int64)Now.ToUnixTimestamp() * 1000000 + (int64)Now.GetMillisecond() * 1000;

    ISocketSubsystem* SS = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
    if (SS)
    {
        Socket = SS->CreateSocket(NAME_DGram, TEXT("OgsaTelemetry"), false);
        if (Socket)
        {
            Socket->SetMulticastLoopback(true);
            Socket->SetMulticastTtl(TTL);
            if (!LocalInterfaceAddress.IsEmpty())
            {
                TSharedRef<FInternetAddr> IfAddr = SS->CreateInternetAddr();
                bool bIfValid = false;
                IfAddr->SetIp(*LocalInterfaceAddress, bIfValid);
                if (bIfValid) Socket->SetMulticastInterface(*IfAddr);
            }
            TSharedRef<FInternetAddr> Addr = SS->CreateInternetAddr();
            Addr->SetPort((uint32)Port);
            bool bIsValid = false;
            Addr->SetIp(*MulticastGroup, bIsValid);
            if (bIsValid) Endpoint = Addr;
        }
    }
}

void UTelemetryPublisherComponent::EndPlay(const EEndPlayReason::Type Reason)
{
    if (Socket)
    {
        ISocketSubsystem* SS = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
        if (SS) SS->DestroySocket(Socket);
        Socket = nullptr;
    }
    Super::EndPlay(Reason);
}

FOgsaChannelHandle UTelemetryPublisherComponent::DeclareChannel(
    const FString& Name, const FString& Unit,
    double Min, double Max, bool HasRange)
{
    if (bLocked)
    {
        // runtime guard, not checkf: check macros compile out in Shipping,
        // where a late declaration would silently append a channel that
        // receivers (which cache descriptors by id) never discover.
        // The rejected handle keeps Id = -1; bind/push calls reject it.
        UE_LOG(LogTemp, Error,
            TEXT("OGSA-TS: DeclareChannel('%s') after the first tick — declarations are locked (receivers cache descriptors by id)"),
            *Name);
        return FOgsaChannelHandle();
    }
    ChannelDecl D;
    D.Name = TCHAR_TO_UTF8(*Name);
    D.Unit = TCHAR_TO_UTF8(*Unit);
    D.Min = Min; D.Max = Max; D.HasRange = HasRange;
    Declarations.Add(D);
    Bindings.Add({});
    FOgsaChannelHandle H; H.Id = Declarations.Num() - 1;
    return H;
}

FOgsaChannelHandle UTelemetryPublisherComponent::DeclareBooleanChannel(
    const FString& Name)
{
    FOgsaChannelHandle H = DeclareChannel(Name, TEXT(""), 0, 0, false);
    if (H.Id >= 0)
        Bindings[H.Id].bBool = true;
    return H;
}

void UTelemetryPublisherComponent::SetChannel(FOgsaChannelHandle Handle,
                                              double Value)
{
    if (Bindings.IsValidIndex(Handle.Id))
    {
        if (Bindings[Handle.Id].bBool)
        {
            // parity with the C# core: type mismatch is rejected, not coerced
            UE_LOG(LogTemp, Error,
                TEXT("OGSA-TS: SetChannel(double) on boolean channel id %d rejected"),
                Handle.Id);
            return;
        }
        Bindings[Handle.Id].Pushed = Value;
    }
}

void UTelemetryPublisherComponent::SetBoolChannel(FOgsaChannelHandle Handle,
                                                  bool Value)
{
    if (Bindings.IsValidIndex(Handle.Id))
    {
        if (!Bindings[Handle.Id].bBool)
        {
            UE_LOG(LogTemp, Error,
                TEXT("OGSA-TS: SetBoolChannel on numeric channel id %d rejected"),
                Handle.Id);
            return;
        }
        Bindings[Handle.Id].bBoolValue = Value;
    }
}

void UTelemetryPublisherComponent::SetPaused(bool bPaused)
{
    bManualPaused = bPaused;
}

void UTelemetryPublisherComponent::SetBodyDynamics(FVector SpecificForce,
                                                   FVector AngularVelocity)
{
    // UE body-axis labels (X surge, Y sway, Z heave) -> standard C-4
    // (x sway, y heave, z surge): the same even cyclic permutation the
    // pose path uses (handedness preserved, pseudovectors included)
    BodySpecificForce = ToStandard(Vec3{ (float)SpecificForce.X,
                                         (float)SpecificForce.Y,
                                         (float)SpecificForce.Z });
    BodyAngularVelocity = ToStandard(Vec3{ (float)AngularVelocity.X,
                                           (float)AngularVelocity.Y,
                                           (float)AngularVelocity.Z });
    bHasBodyDynamics = true;
}

void UTelemetryPublisherComponent::ClearBodyDynamics()
{
    bHasBodyDynamics = false;
}

// standard (x right, y up, z fwd) -> UE (X fwd, Y right, Z up): the inverse
// of ToStandard's cyclic permutation
static FVector ToUnreal(const Vec3& s) { return FVector(s.Z, s.X, s.Y); }

void UTelemetryPublisherComponent::ApplyGravity(const Vec3& StandardGravity)
{
    Environment Next = Env;
    Next.SetGravity(StandardGravity);
    if (Next.Gravity == Env.Gravity && Next.HasGravityVector == Env.HasGravityVector &&
        Next.GravityVector.X == Env.GravityVector.X &&
        Next.GravityVector.Y == Env.GravityVector.Y &&
        Next.GravityVector.Z == Env.GravityVector.Z) return;
    Env = Next;
    LastDiscovery = -1e18; // re-send on change (C-12)
}

void UTelemetryPublisherComponent::SetGravity(FVector Gravity)
{
    ApplyGravity(ToStandard(Vec3{ (float)Gravity.X, (float)Gravity.Y, (float)Gravity.Z }));
}

void UTelemetryPublisherComponent::SetLocalGravity(FVector Gravity)
{
    LocalGravity = ToStandard(Vec3{ (float)Gravity.X, (float)Gravity.Y, (float)Gravity.Z });
    bHasLocalGravity = true;
}

void UTelemetryPublisherComponent::ClearLocalGravity()
{
    bHasLocalGravity = false;
}

FVector UTelemetryPublisherComponent::GetEffectiveGravity() const
{
    return ToUnreal(bHasLocalGravity ? LocalGravity : Env.GravityWorld());
}

bool UTelemetryPublisherComponent::SourceFactor(const FString& From,
                                                const FString& To,
                                                double& OutFactor,
                                                double& OutOffset)
{
    // Mirrors Conversions.TryGetFactor in the C# core (ogsa-ts-dotnet):
    // same pairs, same factors. Wire strings per UnitInfo.ToWireString.
    // Keep both tables in lockstep — the engines must provably agree.
    OutOffset = 0;
    OutFactor = 1.0;
    if (From == To) return true;
    auto Is = [](const FString& A, const char* B) {
        return A.Equals(UTF8_TO_TCHAR(B), ESearchCase::IgnoreCase);
    };
    auto Pair = [&](const char* F, const char* T, double Factor) {
        if (Is(From, F) && Is(To, T)) { OutFactor = Factor; return true; }
        return false;
    };
    // speed
    if (Pair("km/h", "m/s", 1.0 / 3.6)) return true;
    if (Pair("mph",  "m/s", 0.44704)) return true;
    if (Pair("ft/s", "m/s", 0.3048)) return true;
    if (Pair("kn",   "m/s", 1852.0 / 3600.0)) return true;
    // length
    if (Pair("ft",   "m",   0.3048)) return true;
    // angle
    if (Pair("deg",  "rad", PI / 180.0)) return true;
    if (Pair("deg/s", "rad/s", PI / 180.0)) return true;
    // rotation rate
    if (Pair("rps",  "rpm", 60.0)) return true;
    // acceleration (g-force)
    if (Pair("g",    "m/s^2", 9.80665)) return true;
    // acceleration (ft/s^2)
    if (Pair("ft/s^2", "m/s^2", 0.3048)) return true;
    // mass (gram)
    if (Pair("g",    "kg",  1e-3)) return true;
    // pressure
    if (Pair("bar",  "Pa",  1e5)) return true;
    if (Pair("psi",  "Pa",  6894.757293168361)) return true;
    if (Pair("kPa",  "Pa",  1000.0)) return true;
    // ratio
    if (Pair("%",    "",    1e-2)) return true;
    // temperature (affine)
    if (Is(From, "degF") && Is(To, "degC"))
    {
        OutOffset = -32.0 / 1.8;
        OutFactor = 5.0 / 9.0;
        return true;
    }
    // degC -> degF is unreachable today (Fahrenheit is not a legal target
    // unit, so BindChannel never requests this direction), but the C# core's
    // Conversions.TryGetConverter implements both directions — mirrored here
    // so the tables stay in lockstep if that whitelist ever changes.
    if (Is(From, "degC") && Is(To, "degF"))
    {
        OutOffset = 32.0;
        OutFactor = 9.0 / 5.0;
        return true;
    }
    return false;
}

void UTelemetryPublisherComponent::BindChannel(
    const FOgsaChannelHandle& Handle, Getter Source, const FString& SourceUnit)
{
    if (!Bindings.IsValidIndex(Handle.Id)) return;
    double factor = 1.0, offset = 0.0;
    const FString Target(UTF8_TO_TCHAR(Declarations[Handle.Id].Unit.c_str()));
    if (!SourceFactor(SourceUnit, Target, factor, offset))
    {
        // Parity with the C# core: BindChannel throws on unknown pairs.
        // Reject the bind (channel falls back to the push model) instead
        // of silently emitting wrong units on the wire.
        UE_LOG(LogTemp, Error,
            TEXT("OGSA-TS: no conversion %s -> %s for channel '%s' (id %d); "
                 "bind rejected — convert inside the getter or use BindChannelRaw"),
            *SourceUnit, *Target,
            *FString(UTF8_TO_TCHAR(Declarations[Handle.Id].Name.c_str())),
            Handle.Id);
        return;
    }
    Bindings[Handle.Id].Get = MoveTemp(Source);
    Bindings[Handle.Id].Factor = factor;
    Bindings[Handle.Id].Offset = offset;
}

void UTelemetryPublisherComponent::BindChannelRaw(
    const FOgsaChannelHandle& Handle, Getter Source)
{
    if (!Bindings.IsValidIndex(Handle.Id)) return;
    Bindings[Handle.Id].Get = MoveTemp(Source);
    Bindings[Handle.Id].Factor = 1.0;
    Bindings[Handle.Id].Offset = 0.0;
}

void UTelemetryPublisherComponent::FireEvent(const FString& Name,
    float Intensity, float Duration, FVector Direction)
{
    EventInput E;
    E.Id = NextEventId++;
    E.Name = TCHAR_TO_UTF8(*Name);
    E.Intensity = Intensity;
    E.Duration = Duration;
    // UE LH (X-fwd, Y-right, Z-up) -> standard (Z-fwd, X-right, Y-up)
    Vec3 d = ToStandard(Vec3{ (float)Direction.X, (float)Direction.Y,
                              (float)Direction.Z });
    E.Direction = d;
    E.HasDirection = !Direction.IsNearlyZero();
    RecentEvents.Add({ E, Clock });
}

void UTelemetryPublisherComponent::GetCurrentValue(int32 Index,
    double& OutValue, bool& OutBool) const
{
    const FBinding& B = Bindings[Index];
    if (B.Get)
    {
        double v = B.Get();
        OutValue = v * B.Factor + B.Offset;
        OutBool = OutValue != 0;
    }
    else
    {
        OutValue = B.Pushed;
        OutBool = B.bBoolValue;
    }
}

void UTelemetryPublisherComponent::Send(FrameType Type,
                                        const std::vector<uint8_t>& Datagram)
{
    if (!Socket || !Endpoint.IsValid()) return;
    int32 Sent = 0;
    Socket->SendTo((const uint8_t*)Datagram.data(),
                   (int32)Datagram.size(), Sent, *Endpoint);
}

void UTelemetryPublisherComponent::SendDiscovery()
{
    WireBuilder B;
    ObjectDecl Obj;
    Obj.Name = TCHAR_TO_UTF8(*ObjectName);
    Obj.Location = "local";
    Obj.TypeLabel = TCHAR_TO_UTF8(*TypeLabel);
    Obj.Type = 1; // vehicle (ObjectType ordinal: 0 other, 1 vehicle, ...)
    Obj.Primary = true;
    Obj.Channels.clear();
    Obj.Channels.reserve((size_t)Declarations.Num());
    for (const auto& D : Declarations) Obj.Channels.push_back(D);
    BuildDiscovery(B, 1, SessionStartUnixUs,
        TCHAR_TO_UTF8(*(GameName.IsEmpty()
            ? FApp::GetProjectName() : GameName)),
        Env, { Obj });
    Send(FrameType::Discovery, B.Wrap(FrameType::Discovery));
}

void UTelemetryPublisherComponent::SendMotion()
{
    WireBuilder B;
    MotionObjectInput O;
    O.Name = TCHAR_TO_UTF8(*ObjectName);

    if (bAutoBindRootPose && GetOwner())
    {
        const FTransform T = GetOwner()->GetActorTransform();
        const FVector P = T.GetTranslation();     // cm -> m, UE -> standard
        O.Position = ToStandard(Vec3{ (float)P.X, (float)P.Y, (float)P.Z });
        O.Position.X /= 100.f; O.Position.Y /= 100.f; O.Position.Z /= 100.f;
        const FQuat Q = T.GetRotation();
        O.Orientation = ToStandard({ (float)Q.X, (float)Q.Y,
                                     (float)Q.Z, (float)Q.W });
    }

    // C-11: nothing is measured while the simulation is stopped, and a
    // derivative across the pause boundary is meaningless
    if (bHasBodyDynamics && !bPausedNow)
    {
        O.HasBodyDynamics = true;
        O.SpecificForce = BodySpecificForce;
        O.AngularVelocity = BodyAngularVelocity;
    }
    // C-12: local gravity describes its own frame, so it rides every
    // motion frame while set
    O.HasLocalGravity = bHasLocalGravity;
    O.LocalGravity = LocalGravity;

    for (int32 i = 0; i < Declarations.Num(); i++)
    {
        double v = 0; bool b = false;
        GetCurrentValue(i, v, b);
        ChannelValueInput C;
        C.Id = (uint16_t)i;
        if (Bindings[i].bBool) { C.Boolean = b; C.IsBool = true; }
        else C.Number = v;
        O.Channels.push_back(C);
    }
    BuildMotion(B, Clock, { O });
    Send(FrameType::Motion, B.Wrap(FrameType::Motion));
}

void UTelemetryPublisherComponent::SendEvents()
{
    if (RecentEvents.IsEmpty()) return; // check before building the vector
    WireBuilder B;
    std::vector<EventInput> Evs;
    for (const auto& Pair : RecentEvents)
        Evs.push_back(Pair.Key);
    BuildEventFrame(B, Clock, Evs);
    Send(FrameType::Event, B.Wrap(FrameType::Event));
}

void UTelemetryPublisherComponent::TickComponent(
    float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTick)
{
    Super::TickComponent(DeltaTime, TickType, ThisTick);
    // real time, not DeltaTime: dilation and pause must not slow or stop
    // the stream (C-11)
    const double Now = FPlatformTime::Seconds();
    if (ClockOrigin < 0) ClockOrigin = Now;
    Clock = Now - ClockOrigin;
    bLocked = true;

    const UWorld* World = GetWorld();
    bPausedNow = bManualPaused || (bAutoDetectPause && World && World->IsPaused());
    if (Bindings.IsValidIndex(PauseChannel.Id))
        Bindings[PauseChannel.Id].bBoolValue = bPausedNow;

    // C-12: the world's gravity (cm/s² along UE Z; negative = down)
    if (bGravityFromWorld && World)
    {
        const float Gz = World->GetGravityZ();
        if (!bWorldGravityKnown || Gz != LastWorldGravityZ)
        {
            bWorldGravityKnown = true;
            LastWorldGravityZ = Gz;
            ApplyGravity(ToStandard(Vec3{ 0.f, 0.f, Gz / 100.f }));
        }
    }

    if (Clock - LastDiscovery >= DiscoveryIntervalSeconds)
    {
        SendDiscovery();
        LastDiscovery = Clock;
    }
    if (Clock - LastMotion >= 1.0 / FMath::Max(MotionHz, 1.0))
    {
        SendMotion();
        LastMotion = Clock;
    }
    while (!RecentEvents.IsEmpty() && Clock - RecentEvents[0].Value > 0.100)
        RecentEvents.RemoveAt(0);
    if (!RecentEvents.IsEmpty() && Clock - LastEventSend >= 0.050)
    {
        SendEvents();
        LastEventSend = Clock;
    }
}
