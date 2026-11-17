"""Forward the shared object display-name function to its native implementation."""

from pathlib import Path
import shutil

import unreal


ASSET = "/Game/Blueprints/Data/BFL_HelpfulFunctions"


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
        raise RuntimeError("Load the clean shared library and build its native implementation.")
    graphs = {graph.get_name(): graph for graph in editor.list_graphs(asset)}
    graph = graphs.get("GetObjectNames")
    if not graph:
        raise RuntimeError("Missing GetObjectNames graph.")
    forward = unreal.SampleBlueprintMigrationLibrary.forward_object_names
    if forward(asset, graph, native, True):
        unreal.log("Native object display-name function already installed.")
        return

    package = project.parent / "Content/Blueprints/Data/BFL_HelpfulFunctions.uasset"
    backup = project.parent / "Saved/BlueprintMigration/SharedObjectNames" / package.name
    if backup.exists() and backup.read_bytes() != package.read_bytes():
        raise RuntimeError(f"Shared object-name backup differs from the current asset: {backup}")
    backup.parent.mkdir(parents=True, exist_ok=True)
    if not backup.exists():
        shutil.copy2(package, backup)
    if not forward(asset, graph, native):
        raise RuntimeError(f"Unexpected GetObjectNames graph or native signature. Original copy: {backup}")
    if not editor.compile_blueprint(asset):
        raise RuntimeError(f"Object-name function compilation failed. Original copy: {backup}")
    graph = next((item for item in editor.list_graphs(asset) if item.get_name() == "GetObjectNames"), None)
    if not graph or not forward(asset, graph, native, True):
        raise RuntimeError(f"Object-name function verification failed. Original copy: {backup}")
    assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
    if not assets.save_loaded_asset(asset, only_if_is_dirty=False):
        raise RuntimeError(f"Object-name function save failed. Original copy: {backup}")
    unreal.log("Native object display-name function installed.")


if __name__ == "__main__":
    migrate()
