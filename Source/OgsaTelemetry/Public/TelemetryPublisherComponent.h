// TelemetryPublisherComponent.h — OGSA-TS publisher for one vehicle/object.
// Configure transport and cadence in Blueprint; declare channels in
// BeginPlay (declarations lock at the first tick); bind values from C++
// (pull model) or push per tick (Blueprint-friendly). Pose auto-binds to
// the owner root component; UE's frame (LH, X-fwd, Y-right, Z-up) converts
// to the standard's (LH, Z-fwd, X-right, Y-up) in OgsaTelemetryFrame.h.
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
    int64 SessionStartUnixUs = 0;
    bool bLocked = false;

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
