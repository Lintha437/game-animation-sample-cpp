using UnrealBuildTool;

public class GameAnimationSampleCpp : ModuleRules
{
    public GameAnimationSampleCpp(ReadOnlyTargetRules target) : base(target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PrivateDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine", "AnimGraphRuntime", "DrawDebugLibrary", "EnhancedInput", "GameplayCameras", "GameplayTags", "InputCore", "Mover", "NetCore", "PhysicsControl" });
        PrivateDependencyModuleNames.AddRange(new[] { "AIModule", "GameplayInteractionsModule", "GameplayStateTreeModule", "GameplayTasks", "Landscape", "MediaAssets", "NavigationSystem", "SmartObjectsModule", "StateTreeModule", "UMG" });
    }
}
