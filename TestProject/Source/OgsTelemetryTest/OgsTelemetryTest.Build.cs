using UnrealBuildTool;

public class OgsTelemetryTest : ModuleRules
{
    public OgsTelemetryTest(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new[]
            { "Core", "CoreUObject", "Engine", "InputCore" });
    }
}
