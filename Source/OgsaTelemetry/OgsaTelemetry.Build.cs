using UnrealBuildTool;

public class OgsaTelemetry : ModuleRules
{
    public OgsaTelemetry(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(
            new[] { "Core", "CoreUObject", "Engine", "Sockets", "OgsaTelemetryCore" });
    }
}
