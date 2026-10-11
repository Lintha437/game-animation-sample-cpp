"""Reparent the Look At POI target gizmo and retain its authored tooltip."""

from hashlib import sha256
from pathlib import Path
import shutil

import unreal


ASSET = "/Game/IsolatedExamples/LookAtPOI/TestLevelContent/Props/BP_LookAtPOI_TargetGizmo"
REFERENCE = "45c67062f9592b0b2997fecae05d01758a0beecd59a6bd67d6ca5e7bfdc59961"
RETIRED_GRAPHS = {"UserConstructionScript", "EventGraph", "Set Color", "Buffer Mouse Pos"}


def migrate():
    descriptor = Path(unreal.Paths.get_project_file_path()).resolve()
    if descriptor.name != "GameAnimSampleCpp.uproject":
        raise RuntimeError("Run the migration in the native project.")
    editor = unreal.BlueprintEditorLibrary
    asset = unreal.load_asset(ASSET)
    native = unreal.load_class(None, "/Script/GameAnimationSampleCpp.SampleLookAtGizmo")
    dirty = {item.get_name() for item in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()}
    if not asset or not native or ASSET in dirty:
        raise RuntimeError("Cannot load a clean Look At gizmo and its native parent.")
    parent = editor.get_blueprint_parent_class(asset)
    if parent == native:
        unreal.log("Look At gizmo already migrated.")
        return

    package = descriptor.parent / "Content" / (ASSET.removeprefix("/Game/") + ".uasset")
    graphs = {graph.get_name(): graph for graph in editor.list_graphs(asset)}
    if (parent != unreal.Actor.static_class() or not editor.can_reparent_blueprint(asset, native)
            or set(graphs) != RETIRED_GRAPHS | {"Set Parallel To Camera & Show Tooltip"}
            or sha256(package.read_bytes()).hexdigest() != REFERENCE):
        raise RuntimeError("The Look At gizmo differs from the reference.")
    backup = descriptor.parent / "Saved/BlueprintMigration/LookAtGizmo" / package.name
    if backup.exists() and sha256(backup.read_bytes()).hexdigest() != REFERENCE:
        raise RuntimeError(f"The Look At gizmo backup differs from the reference: {backup}")
    backup.parent.mkdir(parents=True, exist_ok=True)
    if not backup.exists():
        shutil.copy2(package, backup)

    for name in sorted(RETIRED_GRAPHS):
        editor.remove_graph(asset, graphs[name])
    editor.reparent_blueprint(asset, native)
    if editor.get_blueprint_parent_class(asset) != native or not editor.compile_blueprint(asset):
        raise RuntimeError("The Look At gizmo failed to compile.")
    assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
    if not assets.save_loaded_asset(asset, only_if_is_dirty=False):
        raise RuntimeError(f"Could not save the Look At gizmo. Previous copy: {backup}")
    unreal.log("Native Look At gizmo installed.")


if __name__ == "__main__":
    migrate()
