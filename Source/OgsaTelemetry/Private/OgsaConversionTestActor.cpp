// OgsaConversionTestActor.cpp — see header.
//
// Expected wire values in the TelemetryReceive motion line (object
// 'conversion_test'), every ~3 s while PIE runs:
//   0=20.00 1=100.00 2=1.00 3=1.50 4=0.00 5=1.00 6=101325.00
// Channel 4 must be 0.00 (rejected bind falls back to push-default), and
// the UE Output Log carries the rejection error for 't.bad'.
#include "OgsaConversionTestActor.h"
#include "Components/SceneComponent.h"

AOgsaConversionTestActor::AOgsaConversionTestActor()
{
    PrimaryActorTick.bCanEverTick = false;
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    Telemetry = CreateDefaultSubobject<UTelemetryPublisherComponent>(
        TEXT("Telemetry"));
    Telemetry->ObjectName = TEXT("conversion_test");
}

void AOgsaConversionTestActor::BeginPlay()
{
    Super::BeginPlay(); // component (incl. socket) initializes first

    // id  channel   declared  bind source  getter   expected on wire
    // 0   t.speed   m/s       km/h         72       20.00  (factor 1/3.6)
    // 1   t.temp    degC      degF         212      100.00 (affine)
    // 2   t.len     m         ft           3.28084  1.00   (feet row)
    // 3   t.mass    kg        g            1500     1.50   (gram row)
    // 4   t.bad     m/s       deg          5        0.00   (REJECTED: bind
    //                                              falls back to push 0)
    // 5   t.accel   m/s^2     ft/s^2       3.28084  1.00   (ft/s^2 row)
    // 6   t.opress  Pa        kPa          101.325  101325.00 (kPa row)
    const auto Speed = Telemetry->DeclareChannel(TEXT("t.speed"), TEXT("m/s"), 0, 100, true);
    const auto Temp  = Telemetry->DeclareChannel(TEXT("t.temp"), TEXT("degC"));
    const auto Len   = Telemetry->DeclareChannel(TEXT("t.len"), TEXT("m"));
    const auto Mass  = Telemetry->DeclareChannel(TEXT("t.mass"), TEXT("kg"));
    const auto Bad   = Telemetry->DeclareChannel(TEXT("t.bad"), TEXT("m/s"));
    const auto Accel = Telemetry->DeclareChannel(TEXT("t.accel"), TEXT("m/s^2"));
    const auto OPress = Telemetry->DeclareChannel(TEXT("t.opress"), TEXT("Pa"));

    Telemetry->BindChannel(Speed, [] { return 72.0; },    TEXT("km/h"));
    Telemetry->BindChannel(Temp,  [] { return 212.0; },   TEXT("degF"));
    Telemetry->BindChannel(Len,   [] { return 3.28084; }, TEXT("ft"));
    Telemetry->BindChannel(Mass,  [] { return 1500.0; },  TEXT("g"));
    Telemetry->BindChannel(Bad,   [] { return 5.0; },     TEXT("deg"));
    Telemetry->BindChannel(Accel, [] { return 3.28084; }, TEXT("ft/s^2"));
    Telemetry->BindChannel(OPress, [] { return 101.325; }, TEXT("kPa"));
}
