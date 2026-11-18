using UnrealBuildTool;

public class GameAnimationSampleCppEditor : ModuleRules
{
    public GameAnimationSampleCppEditor(ReadOnlyTargetRules target) : base(target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PrivateDependencyModuleNames.AddRange(new[] {
            "Core", "CoreUObject", "Engine", "BlueprintGraph", "EnhancedInput", "GameAnimationSampleCpp", "InputBlueprintNodes", "InputCore", "Mover", "UnrealEd",
        });
    }
}
