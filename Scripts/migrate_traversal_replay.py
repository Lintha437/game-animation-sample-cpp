"""Install native recipient handling behind the sample traversal multicast."""

from pathlib import Path
import shutil

import unreal


ASSET = "/Game/Blueprints/AC_TraversalLogic"
PARENT = "/Script/GameAnimationSampleCpp.SampleTraversalComponent"
SERVER = "PerformTraversalAction_Server"
CLIENTS = "PerformTraversalAction_Clients"


def migrate():
    descriptor = Path(unreal.Paths.get_project_file_path()).resolve()
    if descriptor.name != "GameAnimSampleCpp.uproject":
        raise RuntimeError("Run this migration in the native sample project.")
    editor = unreal.BlueprintEditorLibrary
    asset = unreal.load_asset(ASSET)
    parent = editor.get_blueprint_parent_class(asset) if asset else None
    if not parent or parent.get_path_name() != PARENT:
        raise RuntimeError("Apply the traversal movement migration first.")

    def is_dirty():
        return any(package.get_name() == ASSET
                   for package in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages())

    if is_dirty():
        raise RuntimeError("The traversal component has unsaved changes.")
    graphs = {graph.get_name(): graph for graph in editor.list_graphs(asset)}
    migration = unreal.SampleBlueprintMigrationLibrary
    flags = {name: migration.get_compiled_function_flags(asset, name) for name in (SERVER, CLIENTS)}
    if not all(flags.values()) or not migration.replace_traversal_replay(asset, graphs.get("EventGraph")):
        raise RuntimeError("The traversal multicast could not be converted. Discard unsaved changes.")
    if not is_dirty():
        unreal.log("Traversal replay already migrated.")
        return

    package = descriptor.parent / "Content/Blueprints/AC_TraversalLogic.uasset"
    backup = descriptor.parent / "Saved/BlueprintMigration/TraversalReplay" / package.name
    backup.parent.mkdir(parents=True, exist_ok=True)
    if backup.exists() and backup.read_bytes() != package.read_bytes():
        raise RuntimeError(f"A different traversal backup already exists: {backup}")
    if not backup.exists():
        shutil.copy2(package, backup)
    if not editor.compile_blueprint(asset):
        raise RuntimeError(f"The traversal component did not compile. Previous copy: {backup}")
    if any(migration.get_compiled_function_flags(asset, name) != value for name, value in flags.items()):
        raise RuntimeError("Traversal RPC flags changed during migration. Discard unsaved changes.")
    if not unreal.get_editor_subsystem(unreal.EditorAssetSubsystem).save_loaded_asset(asset, only_if_is_dirty=False):
        raise RuntimeError(f"Could not save the traversal component. Previous copy: {backup}")
    unreal.log("Native traversal replay installed.")


if __name__ == "__main__":
    migrate()
