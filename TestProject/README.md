# OgsaTelemetryTest — host project for the plugin

This project exists so the plugin's UE module compiles from the repo
alone — no external project needed. The plugin is **not** committed
here; it is synced in from `../Source` (single source of truth):

```powershell
# from the repo root
./sync-plugin.ps1 -Project ./TestProject -Build
```

That mirrors `Source/` + `OgsaTelemetry.uplugin` into
`TestProject/Plugins/OgsaTelemetry` and builds the
`OgsaTelemetryTestEditor` target. Drop `OgsaConversionTestActor` and
`ADemoVehicle` into a map to verify on the wire (pair with
ogsa-ts-dotnet's TelemetryReceive).
