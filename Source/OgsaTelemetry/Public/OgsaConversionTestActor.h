// OgsaConversionTestActor.h — in-engine verification for the unit
// conversion table (SourceFactor), which must stay in lockstep with
// ogsa-ts-dotnet's Conversions. Place in a level, PIE, and pair with
// ogsa-ts-dotnet's TelemetryReceive: expected wire values are documented
// in the .cpp. Includes one intentionally-rejected bind (t.bad) proving
// unknown pairs fail the bind instead of emitting wrong units.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TelemetryPublisherComponent.h"
#include "OgsaConversionTestActor.generated.h"

UCLASS()
class AOgsaConversionTestActor : public AActor
{
    GENERATED_BODY()

public:
    AOgsaConversionTestActor();
    virtual void BeginPlay() override;

    UPROPERTY(VisibleAnywhere, Category = "OGSA")
    UTelemetryPublisherComponent* Telemetry;
};
