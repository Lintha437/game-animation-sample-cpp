using UnrealBuildTool;

public class GameAnimationSampleCppTarget : TargetRules
{
    public GameAnimationSampleCppTarget(TargetInfo target) : base(target)
    {
        Type = TargetType.Game;
        DefaultBuildSettings = BuildSettingsVersion.V8;
        IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_9;
        ExtraModuleNames.Add("GameAnimationSampleCpp");
    }
}
