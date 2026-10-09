using UnrealBuildTool;

public class OgsaTelemetryTest : ModuleRules
{
    public OgsaTelemetryTest(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new[]
            { "Core", "CoreUObject", "Engine", "InputCore" });
    }
}
