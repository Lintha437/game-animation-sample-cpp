"""Install native Mover state queries and teleport in the local sample pawn."""

from hashlib import sha256
from pathlib import Path
import shutil

import unreal


ASSET = "/Game/Blueprints/SandboxCharacter_Mover"
NATIVE_PARENT = "/Script/GameAnimationSampleCpp.SampleMoverCharacter"
REFERENCE_HASH = "ec3f9762d715fb64638d9f79c1714dd59cb865107cc86d74b7a8efdbf6d2361d"
INHERITED = {"Get_Speed", "CustomTeleport"}
QUERIES = {
    "Get_CurrentMovementMode": "ResolveMovementModeValue",
    "Get_Gait": "ResolveGaitValue",
}


def mode_map(editor, asset):
    defaults = unreal.get_default_object(editor.generated_class(asset))
    return sorted((str(key), str(value)) for key, value
                  in defaults.get_editor_property("MovementModeMap").items())


def migrate():
    descriptor = Path(unreal.Paths.get_project_file_path()).resolve()
    if descriptor.name != "GameAnimSampleCpp.uproject":
        raise RuntimeError("Run the migration in the native sample project.")
    if any(package.get_name() == ASSET for package in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()):
        raise RuntimeError("The Mover pawn has unsaved changes.")
    editor = unreal.BlueprintEditorLibrary
    asset = unreal.load_asset(ASSET)
    parent = unreal.load_class(None, NATIVE_PARENT)
    if not asset or not parent:
        raise RuntimeError("Import the sample content and build the Editor target.")

    graphs = {graph.get_name(): graph for graph in editor.list_graphs(asset)}
    old_parent = editor.get_blueprint_parent_class(asset)
    if old_parent == parent and not INHERITED.intersection(graphs) and QUERIES.keys() <= graphs.keys():
        unreal.log("Mover state already migrated.")
        return
    package = descriptor.parent / "Content/Blueprints/SandboxCharacter_Mover.uasset"
    if (old_parent != unreal.Pawn.static_class() or not INHERITED.union(QUERIES) <= graphs.keys()
            or not editor.can_reparent_blueprint(asset, parent)
            or sha256(package.read_bytes()).hexdigest() != REFERENCE_HASH):
        raise RuntimeError("The Mover pawn differs from the supported reference.")

    backup = descriptor.parent / "Saved/BlueprintMigration/MoverState" / package.name
    backup.parent.mkdir(parents=True, exist_ok=True)
    if backup.exists() and backup.read_bytes() != package.read_bytes():
        raise RuntimeError(f"The existing Mover backup differs from the reference package: {backup}")
    if not backup.exists():
        shutil.copy2(package, backup)

    authored_modes = mode_map(editor, asset)
    for name in sorted(INHERITED):
        editor.remove_graph(asset, graphs[name])
    editor.reparent_blueprint(asset, parent)
    for name, function in QUERIES.items():
        if not unreal.SampleBlueprintMigrationLibrary.replace_enum_query(asset, graphs[name], function):
            raise RuntimeError(f"Failed to wire {name}. Previous copy: {backup}")
    if not editor.compile_blueprint(asset) or mode_map(editor, asset) != authored_modes:
        raise RuntimeError(f"The Mover pawn failed to compile or its mode map changed. Previous copy: {backup}")
    if not unreal.get_editor_subsystem(unreal.EditorAssetSubsystem).save_loaded_asset(asset, only_if_is_dirty=False):
        raise RuntimeError(f"Could not save the Mover pawn. Previous copy: {backup}")
    unreal.log("Native Mover state installed.")


if __name__ == "__main__":
    migrate()
