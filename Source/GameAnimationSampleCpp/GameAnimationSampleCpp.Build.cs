using UnrealBuildTool;

public class GameAnimationSampleCpp : ModuleRules
{
    public GameAnimationSampleCpp(ReadOnlyTargetRules target) : base(target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PrivateDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine", "AnimGraphRuntime", "DrawDebugLibrary", "EnhancedInput", "GameplayTags", "InputCore", "Mover", "NetCore", "PhysicsControl" });
        PrivateDependencyModuleNames.AddRange(new[] { "AIModule", "GameplayInteractionsModule", "GameplayStateTreeModule", "GameplayTasks", "Landscape", "NavigationSystem", "SmartObjectsModule", "StateTreeModule", "UMG" });
    }
}
