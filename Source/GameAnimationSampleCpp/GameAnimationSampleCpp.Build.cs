using UnrealBuildTool;

public class GameAnimationSampleCpp : ModuleRules
{
    public GameAnimationSampleCpp(ReadOnlyTargetRules target) : base(target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PrivateDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine", "AnimGraphRuntime", "EnhancedInput", "GameplayTags", "Mover", "NetCore", "PhysicsControl" });
        PrivateDependencyModuleNames.AddRange(new[] { "GameplayInteractionsModule", "SmartObjectsModule", "StateTreeModule" });
    }
}
