# ogsa-ts-unreal

Unreal Engine plugin for the [OGSA Telemetry Standard (OGSA-TS)](../OGSA-Telemetry-Standard)
— broadcast live motion and event telemetry from an Unreal game to motion
platforms, haptic hardware, dashboards, and loggers on the player's local
network, over UDP multicast. Nothing leaves the LAN.

Status: **validated** — serializer conformance 10/10 and end-to-end in UE 5.8 (channels=3, r=10 m circle, quaternion norm holds). Hand-rolled C++ serializer (no Cap.n' Proto
dependency) proven byte-identical against the standard's conformance
corpus via the C# blueprint mirror (`tools/blueprint`, 6/6 MATCH); the C++
transcription and UE module are pending first compile in a UE environment.

## Approach: corpus-first, dependency-free

The serializer reproduces the exact Cap'n Proto wire layout emitted by the
reference stack — reverse-engineered from the golden vectors
(`../OGSA-Telemetry-Standard/conformance/`), proven by `tools/blueprint`
(a C# mirror that byte-matches all six vectors), and transcribed to C++
in `Source/OgsaTelemetryCore`. UE games ship **zero third-party
libraries**: ~2 headers of std-only C++.

The standalone conformance test needs no engine:

```
cd Test
cmake -B build && cmake --build build
./build/ogsa_conformance ../..(path to)/OGSA-Telemetry-Standard/conformance
```

Expected: 6 corpus PASS + 4 frame-conversion PASS, exit 0. Run it after
any serializer change and after every corpus regeneration.

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
owner root (world transform, cm→m, UE frame → OGSA-TS frame).

## Frame conversion (this engine needs it)

UE is left-handed, X-forward, Y-right, Z-up; OGSA-TS is left-handed,
Z-forward, X-right, Y-up. The map is the cyclic permutation
`(x,y,z)_std = (y,z,x)_ue` (even permutation — handedness preserved), and
orientation converts by `q_std = q_p⁻¹ · q_ue · q_p` with `q_p = (0.5, 0.5, 0.5, 0.5)` (the component map is the inverse cycle of q_p's rotation) — verified in the conformance
test (identity fixed, UE yaw → standard yaw, norm preserved).

## Layout

| Path | Contents |
|---|---|
| `Source/OgsaTelemetryCore/` | Pure C++: wire encoding + frame conversion (no engine deps) |
| `Source/OgsaTelemetry/` | UE module: `UTelemetryPublisherComponent`, FSocket multicast sender, unit conversions |
| `Test/` | Standalone conformance binary (corpus + conversion checks) |
| `tools/blueprint/` | C# formula mirror — the proof that the layout math is exact (6/6 MATCH) |
| `docs/wire-layout.md` | The reverse-engineered layout map (pointer encoding, allocation orders, quirks) |

## Wire-layout notes

The reference stack (Capnp.Net.Runtime) encodes struct-typed fields as
pointers and carries allocator-state padding in some structs' tail bytes
(motion-pose's position pad is `ffffffff`). The corpus is the contract —
this serializer mirrors it, including quirks; see `docs/wire-layout.md`.
Cross-implementation compatibility with capnp C++ is an open question
tracked in the knowledge base.
