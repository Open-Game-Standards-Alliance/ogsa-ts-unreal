// TelemetryPublisherComponent.h — OGSA-TS publisher for one vehicle/object.
// Configure transport and cadence in Blueprint; declare channels in
// BeginPlay (declarations lock at the first tick); bind values from C++
// (pull model) or push per tick (Blueprint-friendly). Pose auto-binds to
// the owner root component; UE's frame (LH, X-fwd, Y-right, Z-up) converts
// to the standard's (LH, Z-fwd, X-right, Y-up) in OgsaTelemetryFrame.h.
//
// Pause (standard CONVENTIONS C-11): the component ticks even while the
// game is paused, on a real-time clock, so it keeps streaming and reports
// the pause on session.paused. A paused world (SetGamePaused) is detected
// automatically; games that pause another way call SetPaused.
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "OgsaTelemetry/OgsaTelemetryWire.h"
#include "OgsaTelemetry/OgsaTelemetryFrame.h"
#include "TelemetryPublisherComponent.generated.h"

class FSocket;
class FInternetAddr;

USTRUCT(BlueprintType)
struct FOgsaChannelHandle
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) int32 Id = -1;
};

UCLASS(ClassGroup = (OGSA), meta = (BlueprintSpawnableComponent))
class UTelemetryPublisherComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UTelemetryPublisherComponent();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OGSA|Transport")
    FString MulticastGroup = TEXT("239.255.17.17");
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OGSA|Transport")
    int32 Port = 40123;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OGSA|Transport")
    int32 TTL = 1;
    /// Egress NIC for multicast, as that interface's local IPv4 address
    /// (e.g. "192.168.1.5"). Empty lets the OS default route pick the
    /// interface — on a multi-homed host (dedicated servers, VMs, a dev
    /// machine with a VPN adapter) that can silently pick a NIC with no
    /// path to the LAN.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OGSA|Transport")
    FString LocalInterfaceAddress;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OGSA|Identity")
    FString GameName;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OGSA|Identity")
    FString ObjectName = TEXT("player");
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OGSA|Identity")
    FString TypeLabel;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OGSA|Cadence")
    double MotionHz = 60.0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OGSA|Cadence")
    double DiscoveryIntervalSeconds = 2.0;

    /// Bind the owner root component's world pose automatically.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OGSA|Pose")
    bool bAutoBindRootPose = true;

    /// Report the game as paused (session.paused) while the world is
    /// paused. Games that pause another way call SetPaused; both sources
    /// combine.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OGSA|Pause")
    bool bAutoDetectPause = true;

    /// Declare the world's gravity (GetGravityZ — World Settings / physics)
    /// as the session gravity (standard CONVENTIONS C-12), at start and
    /// whenever it changes. An explicit SetGravity stands until then.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OGSA|Gravity")
    bool bGravityFromWorld = true;

    /// Declares the session gravity: UE world frame, m/s², pointing the way
    /// gravity pulls (standard gravity is (0, 0, -9.80665)). Straight down
    /// rides as the scalar alone; any other direction also as a vector.
    UFUNCTION(BlueprintCallable, Category = "OGSA")
    void SetGravity(FVector Gravity);

    /// Pushes this object's local gravity per frame (UE world frame, m/s²)
    /// for gravity that varies by position or changes faster than
    /// discovery — planet surfaces, rotating habitats. Carried in every
    /// motion frame until ClearLocalGravity; a zero vector is weightless.
    UFUNCTION(BlueprintCallable, Category = "OGSA")
    void SetLocalGravity(FVector Gravity);
    UFUNCTION(BlueprintCallable, Category = "OGSA")
    void ClearLocalGravity();

    /// The gravity the stream declares for this object (local, else
    /// session; UE world frame, m/s²) — compute SetBodyDynamics' specific
    /// force against it (f = a − g).
    UFUNCTION(BlueprintPure, Category = "OGSA")
    FVector GetEffectiveGravity() const;

    /// Reports a pause the engine can't see (a game-managed pause that
    /// leaves the world running). Combines with bAutoDetectPause: the game
    /// is paused if either says so.
    UFUNCTION(BlueprintCallable, Category = "OGSA")
    void SetPaused(bool bPaused);
    /// Whether the component is currently reporting a pause.
    UFUNCTION(BlueprintPure, Category = "OGSA")
    bool IsReportingPause() const { return bPausedNow; }

    // ---- declarations (BeginPlay; locked at first tick) ----
    UFUNCTION(BlueprintCallable, Category = "OGSA")
    FOgsaChannelHandle DeclareChannel(const FString& Name,
                                      const FString& Unit,
                                      double Min = 0, double Max = 0,
                                      bool HasRange = false);
    UFUNCTION(BlueprintCallable, Category = "OGSA")
    FOgsaChannelHandle DeclareBooleanChannel(const FString& Name);

    // ---- push model (values already in the channel's target unit) ----
    UFUNCTION(BlueprintCallable, Category = "OGSA")
    void SetChannel(FOgsaChannelHandle Handle, double Value);
    UFUNCTION(BlueprintCallable, Category = "OGSA")
    void SetBoolChannel(FOgsaChannelHandle Handle, bool Value);

    // ---- body dynamics (native, motion-cueing inputs) ----
    /// Pushes the bodyDynamics variant per tick: specific force in m/s²,
    /// accelerometer-style (f = a − g: +1 g on the up axis at rest — what
    /// an IMU reads), and angular velocity in rad/s, C-4 sense (+x nose-up
    /// pitch, +y nose-right yaw, +z right-side-down roll). Input vectors
    /// use UE's body-axis labels (X surge, Y sway, Z heave) and are
    /// remapped to the standard's (x sway, y heave, z surge) here.
    /// Call ClearBodyDynamics to stop sending the variant (receivers fall
    /// back to deriving rates from pose).
    UFUNCTION(BlueprintCallable, Category = "OGSA")
    void SetBodyDynamics(FVector SpecificForce, FVector AngularVelocity);
    UFUNCTION(BlueprintCallable, Category = "OGSA")
    void ClearBodyDynamics();

    // ---- pull model (C++; source units convert at bind) ----
    using Getter = TFunction<double()>;
    void BindChannel(const FOgsaChannelHandle& Handle, Getter Source,
                     const FString& SourceUnit);
    void BindChannelRaw(const FOgsaChannelHandle& Handle, Getter Source);

    UFUNCTION(BlueprintCallable, Category = "OGSA")
    void FireEvent(const FString& Name, float Intensity = 1.f,
                   float Duration = 0.f, FVector Direction = FVector::ZeroVector);

    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType,
                               FActorComponentTickFunction* ThisTick) override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
    struct FBinding
    {
        Getter Get;             // pull (source units) if set
        double Factor = 1.0;    // precomputed source->target
        double Offset = 0.0;    // e.g. degF->degC
        double Pushed = 0;
        bool bBool = false, bBoolValue = false;
    };
    TArray<ogsa::telemetry::ChannelDecl> Declarations;
    TArray<FBinding> Bindings;
    TArray<TPair<ogsa::telemetry::EventInput, double>> RecentEvents;
    uint32 NextEventId = 1;

    FSocket* Socket = nullptr;
    TSharedPtr<FInternetAddr> Endpoint;
    double LastDiscovery = -1e18, LastMotion = -1e18, LastEventSend = -1e18;
    double Clock = 0;
    double ClockOrigin = -1;    // FPlatformTime at the first tick
    int64 SessionStartUnixUs = 0;
    bool bLocked = false;
    FOgsaChannelHandle PauseChannel;
    bool bManualPaused = false, bPausedNow = false;

    ogsa::telemetry::Vec3 BodySpecificForce, BodyAngularVelocity;
    bool bHasBodyDynamics = false;

    ogsa::telemetry::Environment Env;    // discovery environment (C-12 gravity)
    ogsa::telemetry::Vec3 LocalGravity;  // standard frame, m/s²
    bool bHasLocalGravity = false;
    float LastWorldGravityZ = 0.f;       // cm/s², last declared from the world
    bool bWorldGravityKnown = false;
    void ApplyGravity(const ogsa::telemetry::Vec3& StandardGravity);

    void Send(ogsa::telemetry::FrameType Type,
             const std::vector<uint8_t>& Datagram);
    void SendDiscovery();
    void SendMotion();
    void SendEvents();
    void GetCurrentValue(int32 Index, double& OutValue, bool& OutBool) const;

    /// Wire-string conversion table mirroring the C# core's
    /// Conversions.TryGetFactor (ogsa-ts-dotnet); returns false for
    /// unknown pairs so the bind can be rejected like the C# throw.
    static bool SourceFactor(const FString& From, const FString& To,
                             double& OutFactor, double& OutOffset);
};
