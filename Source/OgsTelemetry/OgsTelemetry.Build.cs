using UnrealBuildTool;

public class OgsTelemetry : ModuleRules
{
    public OgsTelemetry(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(
            new[] { "Core", "CoreUObject", "Engine", "Sockets", "OgsTelemetryCore" });
    }
}
