using UnrealBuildTool;

public class GameAnimationSampleEditorTarget : TargetRules
{
    public GameAnimationSampleEditorTarget(TargetInfo target) : base(target)
    {
        Type = TargetType.Editor;
        DefaultBuildSettings = BuildSettingsVersion.V8;
        IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_9;
        ExtraModuleNames.Add("GameAnimationSample");
    }
}
