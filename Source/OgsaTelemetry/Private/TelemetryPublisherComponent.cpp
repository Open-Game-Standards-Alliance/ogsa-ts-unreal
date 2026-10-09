// TelemetryPublisherComponent.cpp — see header for the API contract.
#include "TelemetryPublisherComponent.h"
#include "Sockets.h"
#include "SocketSubsystem.h"
#include "Misc/App.h"
#include "GameFramework/Actor.h"
#include "Components/SceneComponent.h"

using namespace ogsa::telemetry;

UTelemetryPublisherComponent::UTelemetryPublisherComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
}

void UTelemetryPublisherComponent::BeginPlay()
{
    Super::BeginPlay();

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
    checkf(!bLocked, TEXT("Declarations lock at the first tick (receivers cache descriptors by id)"));
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
    Bindings[H.Id].bBool = true;
    return H;
}

void UTelemetryPublisherComponent::SetChannel(FOgsaChannelHandle Handle,
                                              double Value)
{
    if (Bindings.IsValidIndex(Handle.Id))
        Bindings[Handle.Id].Pushed = Value;
}

void UTelemetryPublisherComponent::SetBoolChannel(FOgsaChannelHandle Handle,
                                                  bool Value)
{
    if (Bindings.IsValidIndex(Handle.Id))
    {
        Bindings[Handle.Id].bBoolValue = Value;
        Bindings[Handle.Id].bBool = true;
    }
}

double UTelemetryPublisherComponent::SourceFactor(const FString& From,
                                                  const FString& To,
                                                  double& OutOffset)
{
    OutOffset = 0;
    if (From == To) return 1.0;
    auto Is = [](const FString& A, const char* B) {
        return A.Equals(UTF8_TO_TCHAR(B), ESearchCase::IgnoreCase);
    };
    if (Is(From, "km/h") && Is(To, "m/s")) return 1.0 / 3.6;
    if (Is(From, "mph") && Is(To, "m/s")) return 0.44704;
    if (Is(From, "kn") && Is(To, "m/s")) return 1852.0 / 3600.0;
    if (Is(From, "deg") && Is(To, "rad")) return PI / 180.0;
    if (Is(From, "deg/s") && Is(To, "rad/s")) return PI / 180.0;
    if (Is(From, "psi") && Is(To, "Pa")) return 6894.757293168361;
    if (Is(From, "bar") && Is(To, "Pa")) return 1e5;
    if (Is(From, "g") && Is(To, "m/s^2")) return 9.80665;
    if (Is(From, "rps") && Is(To, "rpm")) return 60.0;
    if (Is(From, "%") && Is(To, "")) return 1e-2;
    if (Is(From, "degF") && Is(To, "degC")) { OutOffset = -32.0 / 1.8; return 5.0 / 9.0; }
    UE_LOG(LogTemp, Error,
        TEXT("OGSA-TS: no conversion %s -> %s; binding raw"), *From, *To);
    return 1.0;
}

void UTelemetryPublisherComponent::BindChannel(
    const FOgsaChannelHandle& Handle, Getter Source, const FString& SourceUnit)
{
    if (!Bindings.IsValidIndex(Handle.Id)) return;
    double offset = 0;
    double f = SourceFactor(SourceUnit,
        UTF8_TO_TCHAR(Declarations[Handle.Id].Unit.c_str()), offset);
    Bindings[Handle.Id].Get = MoveTemp(Source);
    Bindings[Handle.Id].Factor = f;
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

bool UTelemetryPublisherComponent::GetCurrentValue(int32 Index,
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
    return true;
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
    Obj.Type = 0; // vehicle
    Obj.Primary = true;
    Obj.Channels.clear();
    Obj.Channels.reserve((size_t)Declarations.Num());
    for (const auto& D : Declarations) Obj.Channels.push_back(D);
    BuildDiscovery(B, 1, SessionStartUnixUs,
        TCHAR_TO_UTF8(*(GameName.IsEmpty()
            ? FApp::GetProjectName() : GameName)),
        Environment{}, { Obj });
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
    WireBuilder B;
    std::vector<EventInput> Evs;
    for (const auto& Pair : RecentEvents)
        Evs.push_back(Pair.Key);
    if (Evs.empty()) return;
    BuildEventFrame(B, Clock, Evs);
    Send(FrameType::Event, B.Wrap(FrameType::Event));
}

void UTelemetryPublisherComponent::TickComponent(
    float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTick)
{
    Super::TickComponent(DeltaTime, TickType, ThisTick);
    Clock += DeltaTime;
    bLocked = true;

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
