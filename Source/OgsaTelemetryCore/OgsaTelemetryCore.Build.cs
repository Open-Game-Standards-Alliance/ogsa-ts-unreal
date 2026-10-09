// Pure C++ (no engine dependencies): wire encoding + frame conversion.
// The same sources build standalone for the conformance test (Test/).
using UnrealBuildTool;

public class OgsaTelemetryCore : ModuleRules
{
    public OgsaTelemetryCore(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicIncludePaths.Add("Public/OgsaTelemetry");
        PublicDependencyModuleNames.Add("Core"); // module boilerplate only; headers stay std-only
    }
}
