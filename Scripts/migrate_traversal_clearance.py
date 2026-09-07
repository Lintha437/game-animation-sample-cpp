"""Replace the local traversal collision stages with the native clearance query."""

from pathlib import Path
import shutil

import unreal


ASSET = "/Game/Blueprints/AC_TraversalLogic"
PARENT = "/Script/GameAnimationSampleCpp.SampleTraversalComponent"


def migrate():
    descriptor = Path(unreal.Paths.get_project_file_path()).resolve()
    if descriptor.name != "GameAnimSampleCpp.uproject":
        raise RuntimeError("Run this migration in the native sample project.")
    editor = unreal.BlueprintEditorLibrary
    asset = unreal.load_asset(ASSET)
    parent = editor.get_blueprint_parent_class(asset) if asset else None
    if not parent or parent.get_path_name() != PARENT:
        raise RuntimeError("Apply the traversal movement migration first.")
    if any(package.get_name() == ASSET for package in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()):
        raise RuntimeError("The traversal component has unsaved changes.")

    graphs = {graph.get_name(): graph for graph in editor.list_graphs(asset)}
    graph = graphs.get("TryTraversalAction")
    if not graph:
        raise RuntimeError("The original traversal function is missing.")
    package = descriptor.parent / "Content/Blueprints/AC_TraversalLogic.uasset"
    backup = descriptor.parent / "Saved/BlueprintMigration/TraversalClearance" / package.name
    if not unreal.SampleBlueprintMigrationLibrary.replace_traversal_geometry(asset, graph):
        raise RuntimeError("The original geometry stages could not be replaced. Discard unsaved changes.")
    if not any(package.get_name() == ASSET for package in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()):
        unreal.log("Traversal clearance already migrated.")
        return

    backup.parent.mkdir(parents=True, exist_ok=True)
    if backup.exists() and backup.read_bytes() != package.read_bytes():
        raise RuntimeError(f"A different traversal backup already exists: {backup}")
    if not backup.exists():
        shutil.copy2(package, backup)
    if not editor.compile_blueprint(asset):
        raise RuntimeError(f"The traversal component did not compile. Previous copy: {backup}")
    if not unreal.get_editor_subsystem(unreal.EditorAssetSubsystem).save_loaded_asset(asset, only_if_is_dirty=False):
        raise RuntimeError(f"Could not save the traversal component. Previous copy: {backup}")
    unreal.log("Native traversal clearance installed.")


if __name__ == "__main__":
    migrate()
