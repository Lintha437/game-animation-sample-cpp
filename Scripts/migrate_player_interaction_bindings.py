"""Bind Smart Object input natively behind the sample's authored montage gates."""

from pathlib import Path
import shutil

import unreal


ASSET = "/Game/Blueprints/SmartObjects/AC_SmartObjectAnimation"
PARENT = "/Script/GameAnimationSampleCpp.SampleSmartObjectInteractionComponent"
TOGGLE = "use_native_player_interaction_input"


def migrate():
    descriptor = Path(unreal.Paths.get_project_file_path()).resolve()
    if descriptor.name != "GameAnimSampleCpp.uproject":
        raise RuntimeError("Run this migration in the native sample project.")
    editor = unreal.BlueprintEditorLibrary
    asset = unreal.load_asset(ASSET)
    parent = editor.get_blueprint_parent_class(asset) if asset else None
    dirty = {package.get_name() for package in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()}
    graph = editor.find_event_graph(asset) if asset else None
    if not graph or not parent or parent.get_path_name() != PARENT or ASSET in dirty:
        raise RuntimeError("Load a clean interaction component after migrating its Smart Object requests.")
    migration = unreal.SampleBlueprintMigrationLibrary
    already_enabled = unreal.get_default_object(editor.generated_class(asset)).get_editor_property(TOGGLE)
    if already_enabled:
        if (migration.list_enhanced_input_actions(graph)
                or not migration.replace_player_interaction_input_bindings(asset, graph)):
            raise RuntimeError("The saved interaction input wrappers differ from the native binding layout.")
        unreal.log("Native player interaction input already installed.")
        return

    package = descriptor.parent / f"Content/{ASSET.removeprefix('/Game/')}.uasset"
    backup = descriptor.parent / "Saved/BlueprintMigration/PlayerInteractionBindings" / package.name
    if backup.exists() and backup.read_bytes() != package.read_bytes():
        raise RuntimeError(f"The input backup differs from the current asset: {backup}")
    backup.parent.mkdir(parents=True, exist_ok=True)
    if not backup.exists():
        shutil.copy2(package, backup)
    if not migration.replace_player_interaction_input_bindings(asset, graph) or not editor.compile_blueprint(asset):
        raise RuntimeError("Could not install native interaction input. Discard unsaved changes.")
    unreal.get_default_object(editor.generated_class(asset)).set_editor_property(TOGGLE, True)
    assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
    if not assets.save_loaded_asset(asset, only_if_is_dirty=False):
        raise RuntimeError("Could not save the interaction input wrappers.")
    unreal.log("Native player interaction input installed.")


if __name__ == "__main__":
    migrate()
