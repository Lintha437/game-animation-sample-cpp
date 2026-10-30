"""Bind the Look At sample gameplay buttons in the native widget parent."""

from hashlib import sha256
from pathlib import Path
import shutil

import unreal


ASSET = "/Game/IsolatedExamples/LookAtPOI/TestLevelContent/Widget/WB_LookAtPOI__GUI"
REFERENCE = "17ec4c6dd9284505a0185745df871544f61e027ff472eea05b341bbc1d763aea"


def migrate():
    descriptor = Path(unreal.Paths.get_project_file_path()).resolve()
    if descriptor.name != "GameAnimSampleCpp.uproject":
        raise RuntimeError("Run the migration in the native project.")
    editor = unreal.BlueprintEditorLibrary
    asset = unreal.load_asset(ASSET)
    native = unreal.load_class(None, "/Script/GameAnimationSampleCpp.SampleLookAtInterface")
    dirty = {item.get_name() for item in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()}
    if not asset or not native or ASSET in dirty:
        raise RuntimeError("Cannot load a clean Look At widget and its native parent.")
    parent = editor.get_blueprint_parent_class(asset)
    if parent == native:
        unreal.log("Look At gameplay buttons already migrated.")
        return

    package = descriptor.parent / "Content" / (ASSET.removeprefix("/Game/") + ".uasset")
    graph = editor.find_event_graph(asset)
    if (parent != unreal.UserWidget.static_class() or not graph or not editor.can_reparent_blueprint(asset, native)
            or sha256(package.read_bytes()).hexdigest() != REFERENCE):
        raise RuntimeError("The Look At widget differs from the reference.")
    backup = descriptor.parent / "Saved/BlueprintMigration/LookAtGameplayButtons" / package.name
    if backup.exists() and sha256(backup.read_bytes()).hexdigest() != REFERENCE:
        raise RuntimeError(f"The Look At widget backup differs from the reference: {backup}")
    backup.parent.mkdir(parents=True, exist_ok=True)
    if not backup.exists():
        shutil.copy2(package, backup)

    if not unreal.SampleBlueprintMigrationLibrary.remove_look_at_gameplay_buttons(asset, graph):
        raise RuntimeError("Could not isolate the three button handlers. Discard unsaved changes.")
    editor.reparent_blueprint(asset, native)
    if editor.get_blueprint_parent_class(asset) != native or not editor.compile_blueprint(asset):
        raise RuntimeError("The Look At widget failed to compile.")
    assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
    if not assets.save_loaded_asset(asset, only_if_is_dirty=False):
        raise RuntimeError(f"Could not save the Look At widget. Previous copy: {backup}")
    unreal.log("Native Look At gameplay buttons installed.")


if __name__ == "__main__":
    migrate()
