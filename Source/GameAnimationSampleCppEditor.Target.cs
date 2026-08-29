using UnrealBuildTool;

public class GameAnimationSampleCppEditorTarget : TargetRules
{
    public GameAnimationSampleCppEditorTarget(TargetInfo target) : base(target)
    {
        Type = TargetType.Editor;
        DefaultBuildSettings = BuildSettingsVersion.V8;
        IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_9;
        ExtraModuleNames.Add("GameAnimationSampleCpp");
    }
}
