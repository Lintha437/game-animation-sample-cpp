"""Install native CMC movement hooks in the local traversal component."""

from hashlib import sha256
from pathlib import Path
import shutil

import unreal


ASSET = "/Game/Blueprints/AC_TraversalLogic"
PARENT = "/Script/GameAnimationSampleCpp.SampleTraversalComponent"
REFERENCE_HASH = "e59d65511376e32b3dc4ef09a45b15f407a9b658ef015478ddc99b28c673f11b"
NATIVE_GRAPHS = {"SetMovementMode_CMC", "GetEndMovementMode_CMC", "SetReplicationBehavior"}
RETAINED_GRAPHS = {"EventGraph", "TryTraversalAction", "SetWarpTargets"}


def migrate():
    descriptor = Path(unreal.Paths.get_project_file_path()).resolve()
    if descriptor.name != "GameAnimSampleCpp.uproject":
        raise RuntimeError("Run the migration in the native sample project.")
    if any(package.get_name() == ASSET for package in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()):
        raise RuntimeError("The traversal component has unsaved changes.")

    editor = unreal.BlueprintEditorLibrary
    asset = unreal.load_asset(ASSET)
    parent = unreal.load_class(None, PARENT)
    if not asset or not parent:
        raise RuntimeError("Import the reference sample and build the native Editor target.")
    graphs = {graph.get_name(): graph for graph in editor.list_graphs(asset)}
    if not RETAINED_GRAPHS.issubset(graphs):
        raise RuntimeError("The traversal action or animation events are missing.")
    previous = editor.get_blueprint_parent_class(asset)
    if previous == parent and not NATIVE_GRAPHS.intersection(graphs):
        unreal.log("Traversal movement already migrated.")
        return

    package = descriptor.parent / "Content/Blueprints/AC_TraversalLogic.uasset"
    if (
        previous != unreal.ActorComponent.static_class()
        or not NATIVE_GRAPHS.issubset(graphs)
        or sha256(package.read_bytes()).hexdigest() != REFERENCE_HASH
    ):
        raise RuntimeError("The traversal package differs from the supported reference.")
    backup = descriptor.parent / "Saved/BlueprintMigration/TraversalMovement" / package.name
    backup.parent.mkdir(parents=True, exist_ok=True)
    if backup.exists() and backup.read_bytes() != package.read_bytes():
        raise RuntimeError(f"An incompatible traversal backup exists: {backup}")
    if not backup.exists():
        shutil.copy2(package, backup)

    for name in sorted(NATIVE_GRAPHS):
        editor.remove_graph(asset, graphs[name])
    editor.reparent_blueprint(asset, parent)
    if not editor.compile_blueprint(asset):
        raise RuntimeError(f"The traversal component failed to compile. Previous copy: {backup}")
    if any(editor.find_graph(asset, name) != graphs[name] for name in RETAINED_GRAPHS):
        raise RuntimeError(f"A retained traversal graph changed identity. Previous copy: {backup}")
    if not unreal.get_editor_subsystem(unreal.EditorAssetSubsystem).save_loaded_asset(asset, only_if_is_dirty=False):
        raise RuntimeError(f"Failed to save the traversal component. Previous copy: {backup}")
    unreal.log("Native traversal movement installed.")


if __name__ == "__main__":
    migrate()
