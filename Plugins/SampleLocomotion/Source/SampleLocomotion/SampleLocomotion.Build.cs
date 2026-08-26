using UnrealBuildTool;

public class SampleLocomotion : ModuleRules
{
    public SampleLocomotion(ReadOnlyTargetRules target) : base(target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine" });
        PrivateDependencyModuleNames.Add("EnhancedInput");
    }
}
