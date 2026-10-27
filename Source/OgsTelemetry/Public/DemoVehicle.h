// DemoVehicle.h — circling demo vehicle feeding rpm/speed/headlights
// channels. Pair with ogs-ts-dotnet's TelemetryReceive.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TelemetryPublisherComponent.h"
#include "DemoVehicle.generated.h"

UCLASS()
class ADemoVehicle : public AActor
{
    GENERATED_BODY()

public:
    ADemoVehicle();

    UPROPERTY(VisibleAnywhere, Category = "OGS")
    UTelemetryPublisherComponent* Telemetry;

    /// UE works in centimetres; the publisher converts to metres.
    UPROPERTY(EditAnywhere, Category = "OGS")
    float RadiusCm = 1000.f;

    UPROPERTY(EditAnywhere, Category = "OGS")
    float AngularSpeed = 0.8f; // rad/s

    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;

private:
    double T = 0;
    FOgsChannelHandle Rpm, Speed, Headlights;
};
