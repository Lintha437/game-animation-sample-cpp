"""Connect the imported player interaction input to the native slot request."""

from pathlib import Path
import shutil

import unreal


ASSET = "/Game/Blueprints/SmartObjects/AC_SmartObjectAnimation"
PARENT = "/Script/GameAnimationSampleCpp.SampleSmartObjectInteractionComponent"


def migrate():
    descriptor = Path(unreal.Paths.get_project_file_path()).resolve()
    if descriptor.name != "GameAnimSampleCpp.uproject":
        raise RuntimeError("Run the migration in the native project.")
    editor = unreal.BlueprintEditorLibrary
    asset = unreal.load_asset(ASSET)
    parent = editor.get_blueprint_parent_class(asset) if asset else None
    if not parent or parent.get_path_name() != PARENT:
        raise RuntimeError("Apply the Smart Object interaction component migration first.")

    def is_dirty():
        return any(package.get_name() == ASSET
                   for package in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages())

    if is_dirty():
        raise RuntimeError("The interaction component has unsaved changes.")
    graphs = {graph.get_name(): graph for graph in editor.list_graphs(asset)}
    if not {"EventGraph", "Cache Necessary Data"} <= graphs.keys():
        raise RuntimeError("The imported component differs from the reference.")
    migration = unreal.SampleBlueprintMigrationLibrary
    if not migration.replace_player_smart_object_input(asset, graphs["EventGraph"]):
        raise RuntimeError("Could not forward the interaction input. Discard unsaved changes.")
    if not migration.replace_mover_input_function(
        asset, graphs["Cache Necessary Data"], "CacheInteractionComponents", "None"
    ):
        raise RuntimeError("Could not forward the component cache. Discard unsaved changes.")
    if "Find Closest Free Slot" in graphs:
        editor.remove_function_graph(asset, "Find Closest Free Slot")
    if not is_dirty():
        unreal.log("Player Smart Object requests already migrated.")
        return

    package = descriptor.parent / f"Content/{ASSET.removeprefix('/Game/')}.uasset"
    backup = descriptor.parent / "Saved/BlueprintMigration/PlayerSmartObject" / package.name
    backup.parent.mkdir(parents=True, exist_ok=True)
    if backup.exists() and backup.read_bytes() != package.read_bytes():
        raise RuntimeError(f"The interaction component differs from its backup: {backup}")
    if not backup.exists():
        shutil.copy2(package, backup)
    if not editor.compile_blueprint(asset):
        raise RuntimeError(f"Interaction component compile failed. Previous copy: {backup}")
    assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
    if not assets.save_loaded_asset(asset, only_if_is_dirty=False):
        raise RuntimeError(f"Could not save the interaction component. Previous copy: {backup}")
    unreal.log("Native player Smart Object request installed.")


if __name__ == "__main__":
    migrate()
