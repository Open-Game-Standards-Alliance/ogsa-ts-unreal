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
- Products docs describe current state only; history lives in the
  Open-Game-Standards-Alliance knowledge base (knowledge/README.md bridge).
