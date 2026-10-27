// OgsConversionTestActor.h — in-engine verification for the unit
// conversion table (SourceFactor), which must stay in lockstep with
// ogs-ts-dotnet's Conversions. Place in a level, PIE, and pair with
// ogs-ts-dotnet's TelemetryReceive: expected wire values are documented
// in the .cpp. Includes one intentionally-rejected bind (t.bad) proving
// unknown pairs fail the bind instead of emitting wrong units.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TelemetryPublisherComponent.h"
#include "OgsConversionTestActor.generated.h"

UCLASS()
class AOgsConversionTestActor : public AActor
{
    GENERATED_BODY()

public:
    AOgsConversionTestActor();
    virtual void BeginPlay() override;

    UPROPERTY(VisibleAnywhere, Category = "OGS")
    UTelemetryPublisherComponent* Telemetry;
};
