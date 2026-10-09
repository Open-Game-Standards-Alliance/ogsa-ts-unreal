# ogsa-ts-unreal working conventions

- The serializer in Source/OgsaTelemetryCore is CORPUS-LOCKED: it must
  byte-match ../OGSA-Telemetry-Standard/conformance (run Test/ogsa_conformance
  after any change). The layout map and quirks are documented in
  docs/wire-layout.md; the C# proof is tools/blueprint (dotnet run —
  must stay 6/6 MATCH).
- No third-party dependencies ever: the point of the hand-rolled serializer
  is that UE games ship nothing extra. std-only headers in Core.
- Frame conversion (UE LH X-fwd Z-up -> standard LH Z-fwd Y-up) lives in
  OgsaTelemetryFrame.h and must stay covered by the conformance binary's
  conversion checks.
- The unit-conversion table in TelemetryPublisherComponent must stay in
  lockstep with ogsa-ts-dotnet's Conversions (same pairs, same factors).
  Verified in-engine by OgsaConversionTestActor paired with
  TelemetryReceive (expected wire values in the actor .cpp).
- UE test projects consume a *copy* under <project>/Plugins/OgsaTelemetry —
  after changing the plugin run ./sync-plugin.ps1 -Project <path> (add
  -Build to compile the Editor target) so the copy never drifts.
- TestProject/ is the in-repo host project: ./sync-plugin.ps1 -Project
  ./TestProject -Build compiles the UE module from the repo alone (no
  external project). Target rules must use V7 + Unreal5_8 to match the
  engine's own build (common-build-products rule rejects diverging
  warning levels).
- Products docs describe current state only; history lives in the
  Open-Game-Standards-Alliance knowledge base (knowledge/README.md bridge).
