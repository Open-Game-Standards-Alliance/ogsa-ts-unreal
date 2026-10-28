# ogs-ts-unreal

<picture>
  <source media="(prefers-color-scheme: dark)" srcset="assets/telemetry-standard-dark.svg">
  <img align="right" width="120" src="assets/telemetry-standard.svg" alt="OGS Telemetry Standard logo">
</picture>

Unreal Engine plugin for the [OGS Telemetry Standard (OGS-TS)](https://github.com/Open-Game-Standards/OGS-Telemetry-Standard)
— broadcast live motion and event telemetry from an Unreal game to motion
platforms, haptic hardware, dashboards, and loggers on the player's local
network, over UDP multicast. Nothing leaves the LAN.

Community & support: [Discord](https://discord.gg/tseF4Q6U99) · Development is funded by [sponsorship](https://github.com/sponsors/BrianGilbert)

Status: **validated** — end-to-end in UE 5.8 (channels=3, r=10 m circle, quaternion norm holds); UE module compiled and the unit-conversion table verified on the wire against the C# core (`OgsConversionTestActor`, all five probe channels). Dependency-free C++ serializer (no Cap'n Proto
library) byte-identical against the standard's conformance corpus.

**Built with AI assistance.** Significant portions of this plugin
(including the wire serializer, the frame-conversion math,
and the unit-conversion table) were written with AI coding assistance
under human direction and review. Review and test before
relying on it, especially around networking and the serializer/
conversion math.

## Approach: canonical encoding, dependency-free

Frames are written in the standard's **canonical OGS encoding**
(implementation-capnproto.md, Encoding rules): Cap'n Proto canonical form
with fixed field presence, so the bytes depend only on the content. Three
std-only C++17 headers in `Source/OgsTelemetryCore` do it:

- `OgsTelemetrySchema.h` — typed writers generated from the schema by
  `../ogs-ts-dotnet/capnp/gen_writers.py` (field offsets come from the
  official Cap'n Proto compiler; the same generator emits the C# core's
  writers). Never hand-edited.
- `OgsTelemetryCanonical.h` — the canonical encoder the writers build on.
- `OgsTelemetryWire.h` — the engine-facing frame builders and the OMT1
  envelope.

UE games ship **zero third-party libraries**.

The standalone conformance test needs no engine:

```
cd Test
cmake -B build && cmake --build build
./build/ogs_conformance ../..(path to)/OGS-Telemetry-Standard/conformance
```

Expected: every check PASS (corpus vectors byte-identical, frame
conversion, body-dynamics deriver), exit 0. Run it after any serializer
change and after every corpus regeneration.

After a schema change, regenerate the writers (needs `pip install pycapnp`):

```
python ../ogs-ts-dotnet/capnp/gen_writers.py --lang cpp \
  --schema ../OGS-Telemetry-Standard/ogs_telemetry.capnp \
  --out Source/OgsTelemetryCore/Public/OgsTelemetry/OgsTelemetrySchema.h
```

In-engine verification: `sync-plugin.ps1 -Project <path> [-Build]`
mirrors the plugin into a UE project's `Plugins/OgsTelemetry` (and with
`-Build` compiles the Editor target). Place `OgsConversionTestActor` in
a level and watch ogs-ts-dotnet's `TelemetryReceive` — expected wire
values are documented in the actor's .cpp, including one
intentionally-rejected bind that must read `0.00`.

## Quick start (in-engine)

Drop this folder into your project's `Plugins/`, enable the plugin, add
**TelemetryPublisher** to your vehicle, then in BeginPlay:

```cpp
auto Handle = Telemetry->DeclareChannel(TEXT("engine.rpm"), TEXT("rpm"), 0, 9000, true);
Telemetry->BindChannelRaw(Handle, [this]{ return Engine->Rpm; });
// or bind in game-native units (factor precomputed at bind):
Telemetry->BindChannel(SpeedHandle, [this]{ return SpeedKmh; }, TEXT("km/h"));
Telemetry->FireEvent(TEXT("body.impact"), 0.8f, 0.25f, ImpactDirection);
```

Blueprint: `DeclareChannel` / `SetChannel` / `SetBoolChannel` /
`FireEvent` are BlueprintCallable (push model). Pose auto-binds to the
owner root (world transform, cm→m, UE frame → OGS-TS frame).

## Frame conversion (this engine needs it)

UE is left-handed, X-forward, Y-right, Z-up; OGS-TS is left-handed,
Z-forward, X-right, Y-up. The map is the cyclic permutation
`(x,y,z)_std = (y,z,x)_ue` (even permutation — handedness preserved), and
orientation converts by `q_std = q_p⁻¹ · q_ue · q_p` with `q_p = (0.5, 0.5, 0.5, 0.5)` (the component map is the inverse cycle of q_p's rotation) — verified in the conformance
test (identity fixed, UE yaw → standard yaw, norm preserved).

## Layout

| Path | Contents |
|---|---|
| `Source/OgsTelemetryCore/` | Pure C++: generated schema writers, canonical encoder, frame builders, frame conversion (no engine deps) |
| `Source/OgsTelemetry/` | UE module: `UTelemetryPublisherComponent`, FSocket multicast sender, unit conversions |
| `sync-plugin.ps1` | Mirror the plugin into a UE test project (`-Build` compiles it) |
| `Test/` | Standalone conformance binary (corpus + conversion checks) |

## Wire-layout notes

Every datagram is valid Cap'n Proto that any official implementation
decodes, in canonical form: the standard's `conformance/validate.py`
checks the corpus independently, and the official `capnp convert
binary:canonical` leaves every corpus vector unchanged.

## Pause

The publisher keeps streaming while the game is paused and reports the
pause on the ratified `session.paused` channel ([CONVENTIONS C-11](https://github.com/Open-Game-Standards/OGS-Telemetry-Standard/blob/main/CONVENTIONS.md#c-11-pause)), so motion
rigs and haptics come to rest instead of freezing mid-move or dropping
the session. A paused world (`SetGamePaused`) is detected automatically (`bAutoDetectPause`; the component ticks even when paused);
games that pause another way call `SetPaused` (Blueprint-callable). Nothing to wire.

## Wiring your game's values

**C++ (pull model)** — declare in `BeginPlay`, bind lambdas over your
components; pose auto-binds to the owner root (`bAutoBindRootPose`):

```cpp
void AMyVehicle::BeginPlay()
{
    Super::BeginPlay();
    Telemetry = FindComponentByClass<UTelemetryPublisherComponent>();
    Rpm   = Telemetry->DeclareChannel(TEXT("engine.rpm"), TEXT("rpm"), 0, 9000, true);
    Speed = Telemetry->DeclareChannel(TEXT("vehicle.speed"), TEXT("m/s"));
    Gear  = Telemetry->DeclareChannel(TEXT("transmission.gear"), TEXT(""));

    Telemetry->BindChannelRaw(Rpm,  [this]{ return EngineComp->Rpm; });
    Telemetry->BindChannel(Speed, [this]{ return SpeedKmh; }, TEXT("km/h"));
    Telemetry->BindChannelRaw(Gear, [this]{ return (double)Gearbox->GetCurrentGear(); });
}

void AMyVehicle::NotifyHit(UPrimitiveComponent*, AActor*, UPrimitiveComponent*,
                           FVector NormalImpulse, const FHitResult&)
{
    Telemetry->FireEvent(TEXT("body.impact"),
        FMath::Clamp(NormalImpulse.Size() / 100000.f, 0.f, 1.f), 0.25f,
        NormalImpulse.GetSafeNormal());
}
```

**Blueprint-only (push model)** — no C++ needed: add the component, call
`Declare Channel` in BeginPlay (store the handles), then `Set Channel` from
your Blueprint on tick or on change (values must already be in the
channel's declared unit), and `Fire Event` from collision/animation
notifications. C++ pull bindings and Blueprint pushes can coexist on
different channels.
