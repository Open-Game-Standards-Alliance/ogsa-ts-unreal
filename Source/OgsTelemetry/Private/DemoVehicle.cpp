// DemoVehicle.cpp — see header.
#include "DemoVehicle.h"
#include "Components/SceneComponent.h"

ADemoVehicle::ADemoVehicle()
{
    PrimaryActorTick.bCanEverTick = true;
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

    Telemetry = CreateDefaultSubobject<UTelemetryPublisherComponent>(
        TEXT("TelemetryPublisher"));
    Telemetry->ObjectName = TEXT("demo_car");
    Telemetry->TypeLabel = TEXT("OGS-TS UE demo vehicle");
    Telemetry->GameName = TEXT(""); // project name by default
}

void ADemoVehicle::BeginPlay()
{
    Super::BeginPlay(); // components (incl. socket) initialize first

    Rpm = Telemetry->DeclareChannel(TEXT("engine.rpm"), TEXT("rpm"),
                                    0, 9000, true);
    Speed = Telemetry->DeclareChannel(TEXT("vehicle.speed"), TEXT("m/s"),
                                      0, 30, true);
    Headlights = Telemetry->DeclareBooleanChannel(TEXT("lights.headlights"));

    const float radiusM = RadiusCm / 100.f;
    Telemetry->BindChannelRaw(Rpm, [this]
    {
        return 6500.0 * (0.5 + 0.5 * FMath::Sin(T * 2.0));
    });
    Telemetry->BindChannelRaw(Speed, [this, radiusM]
    {
        return AngularSpeed * radiusM; // ω·r, already m/s
    });
    Telemetry->SetBoolChannel(Headlights, true);
}

void ADemoVehicle::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    T += DeltaTime;
    const float Yaw = T * AngularSpeed;
    const FVector Location(FMath::Cos(Yaw) * RadiusCm,
                           FMath::Sin(Yaw) * RadiusCm, 50.f);
    SetActorLocation(Location);
    // face the direction of travel (UE: X forward)
    const FVector Next(FMath::Cos(Yaw + 0.01f) * RadiusCm,
                       FMath::Sin(Yaw + 0.01f) * RadiusCm, 50.f);
    SetActorRotation(FRotationMatrix::MakeFromX(Next - Location).Rotator());
}
