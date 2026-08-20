using UnrealBuildTool;

public class GameAnimationSample : ModuleRules
{
    public GameAnimationSample(ReadOnlyTargetRules target) : base(target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PrivateDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine" });
    }
}
