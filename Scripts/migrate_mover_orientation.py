"""Install native facing, floor, and finalization hooks on the local Mover pawn."""

from pathlib import Path
import shutil

import unreal


ASSET = "/Game/Blueprints/SandboxCharacter_Mover"
PARENT = "/Script/GameAnimationSampleCpp.SampleMoverCharacter"
FORWARDS = {
    "Get_OrientationIntent": ("ResolveOrientationIntent", "ReturnValue"),
    "Update_FloorValues": ("UpdateMoverFloor", "None"),
    "On_BasedMovementApplied": ("CaptureBasedMovement", "None"),
    "On_MovementPostFinalize": ("NotifyMoverModeFinalized", "None"),
}


def migrate():
    descriptor = Path(unreal.Paths.get_project_file_path()).resolve()
    if descriptor.name != "GameAnimSampleCpp.uproject":
        raise RuntimeError("Run the migration in the native sample project.")
    editor = unreal.BlueprintEditorLibrary
    asset = unreal.load_asset(ASSET)
    parent = editor.get_blueprint_parent_class(asset) if asset else None
    if not parent or parent.get_path_name() != PARENT:
        raise RuntimeError("Apply the Mover state and input pipeline migrations first.")

    def is_dirty():
        return any(package.get_name() == ASSET
                   for package in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages())

    if is_dirty():
        raise RuntimeError("The Mover pawn has unsaved changes.")
    graphs = {graph.get_name(): graph for graph in editor.list_graphs(asset)}
    if not FORWARDS.keys() <= graphs.keys():
        raise RuntimeError("The required Mover callbacks differ from the reference.")
    migration = unreal.SampleBlueprintMigrationLibrary
    for name, (native, output) in FORWARDS.items():
        if not migration.replace_mover_input_function(asset, graphs[name], native, output):
            raise RuntimeError(f"Could not forward {name}. Discard unsaved changes.")
    if not is_dirty():
        unreal.log("Mover orientation already migrated.")
        return

    package = descriptor.parent / "Content/Blueprints/SandboxCharacter_Mover.uasset"
    backup = descriptor.parent / "Saved/BlueprintMigration/MoverOrientation" / package.name
    backup.parent.mkdir(parents=True, exist_ok=True)
    if backup.exists() and backup.read_bytes() != package.read_bytes():
        raise RuntimeError(f"The Mover backup differs from the current pawn: {backup}")
    if not backup.exists():
        shutil.copy2(package, backup)
    if not editor.compile_blueprint(asset):
        raise RuntimeError(f"The Mover pawn failed to compile. Previous copy: {backup}")
    if not unreal.get_editor_subsystem(unreal.EditorAssetSubsystem).save_loaded_asset(asset, only_if_is_dirty=False):
        raise RuntimeError(f"Could not save the Mover pawn. Previous copy: {backup}")
    unreal.log("Native Mover orientation installed.")


if __name__ == "__main__":
    migrate()
