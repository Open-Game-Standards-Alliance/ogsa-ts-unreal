// Pure C++ (no engine dependencies): wire encoding + frame conversion.
// The same sources build standalone for the conformance test (Test/).
using UnrealBuildTool;

public class OgsTelemetryCore : ModuleRules
{
    public OgsTelemetryCore(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        // Public/ is a default include root; headers live in Public/OgsTelemetry.
        // Headers are std-only; Core is needed just for the module boilerplate.
        PublicDependencyModuleNames.Add("Core");
    }
}
