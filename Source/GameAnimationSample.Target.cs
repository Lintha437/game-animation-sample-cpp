using UnrealBuildTool;

public class GameAnimationSampleTarget : TargetRules
{
    public GameAnimationSampleTarget(TargetInfo target) : base(target)
    {
        Type = TargetType.Game;
        DefaultBuildSettings = BuildSettingsVersion.V8;
        IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_9;
        ExtraModuleNames.Add("GameAnimationSample");
    }
}
