"""Install native drawing and history functions behind the imported debug library."""

from pathlib import Path
import shutil

import unreal


ASSET = "/Game/Blueprints/Data/BFL_HelpfulFunctions"
FORWARDS = {
    "DrawDebugArrowWithCircle": "DrawDirection",
    "DrawDebugAngleThresholds": "DrawHeadingThresholds",
    "DebugDraw_BoolStates": "DrawBooleanRows",
    "DebugDraw_StringArray": "DrawStringRows",
    "DebugDraw_ObjectNameArray": "DrawObjectNames",
    "DebugDraw_MultiLineGraph": "DrawHistoryGraph",
    "AddToStringHistoryArray": "AddStringHistory",
}


def migrate():
    project = Path(unreal.Paths.get_project_file_path()).resolve()
    if project.name != "GameAnimSampleCpp.uproject":
        raise RuntimeError("Run this migration in the native sample project.")

    def is_dirty():
        return any(item.get_name() == ASSET for item in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages())

    editor = unreal.BlueprintEditorLibrary
    asset = unreal.load_asset(ASSET)
    native = unreal.load_class(None, "/Script/GameAnimationSampleCpp.SampleDebugDrawing")
    if not asset or not native or is_dirty():
        raise RuntimeError("Load the clean shared debug library and build its native implementation.")
    graphs = {item.get_name(): item for item in editor.list_graphs(asset)}
    migration = unreal.SampleBlueprintMigrationLibrary
    for name, function in FORWARDS.items():
        if name not in graphs or not migration.replace_void_function_body(asset, graphs[name], native, function):
            raise RuntimeError(f"Could not forward {name}. Discard unsaved changes.")
    if not is_dirty():
        unreal.log("Native shared debug functions already installed.")
        return
    package = project.parent / "Content/Blueprints/Data/BFL_HelpfulFunctions.uasset"
    backup = project.parent / "Saved/BlueprintMigration/SharedDebugDrawing" / package.name
    if backup.exists() and backup.read_bytes() != package.read_bytes():
        raise RuntimeError(f"Shared debug backup differs from the current asset: {backup}")
    backup.parent.mkdir(parents=True, exist_ok=True)
    if not backup.exists():
        shutil.copy2(package, backup)
    assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
    if not editor.compile_blueprint(asset) or not assets.save_loaded_asset(asset, only_if_is_dirty=False):
        raise RuntimeError(f"Could not compile and save the shared debug functions. Previous copy: {backup}")
    unreal.log("Native shared debug functions installed.")


if __name__ == "__main__":
    migrate()
