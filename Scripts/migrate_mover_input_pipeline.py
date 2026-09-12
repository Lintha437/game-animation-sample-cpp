"""Install native input command assembly and snapshot updates on the local Mover pawn."""

from pathlib import Path
import shutil

import unreal


ASSET = "/Game/Blueprints/SandboxCharacter_Mover"
PARENT = "/Script/GameAnimationSampleCpp.SampleMoverCharacter"
FORWARDS = {
    "ProduceInput": ("BuildMoverInputCommand", "InputCmdResult"),
    "CacheInputsFromMover": ("UpdateCachedMoverInputs", "None"),
    "On_PreSimulateTick": ("ApplyMoverCrouchInput", "None"),
    "Get_AimingRotation": ("ResolveAimingRotation", "ReturnValue"),
    "Update_ControlRotationRate": ("UpdateControlYawRate", "None"),
}


def migrate():
    descriptor = Path(unreal.Paths.get_project_file_path()).resolve()
    if descriptor.name != "GameAnimSampleCpp.uproject":
        raise RuntimeError("Run the migration in the native sample project.")
    def is_dirty():
        return any(package.get_name() == ASSET
                   for package in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages())

    if is_dirty():
        raise RuntimeError("The Mover pawn has unsaved changes.")
    editor = unreal.BlueprintEditorLibrary
    asset = unreal.load_asset(ASSET)
    parent = editor.get_blueprint_parent_class(asset) if asset else None
    if not parent or parent.get_path_name() != PARENT:
        raise RuntimeError("Apply the Mover state migration first.")
    graphs = {graph.get_name(): graph for graph in editor.list_graphs(asset)}
    package = descriptor.parent / "Content/Blueprints/SandboxCharacter_Mover.uasset"
    if not FORWARDS.keys() <= graphs.keys() or {"Get_Speed", "CustomTeleport"}.intersection(graphs):
        raise RuntimeError("The locally migrated Mover pawn differs from the supported input graph.")
    backup = descriptor.parent / "Saved/BlueprintMigration/MoverInputPipeline" / package.name
    for name, (native, output) in FORWARDS.items():
        if not unreal.SampleBlueprintMigrationLibrary.replace_mover_input_function(asset, graphs[name], native, output):
            raise RuntimeError(f"Could not forward {name}. Discard unsaved changes.")
    if not is_dirty():
        unreal.log("Mover input pipeline already migrated.")
        return

    backup.parent.mkdir(parents=True, exist_ok=True)
    if backup.exists() and backup.read_bytes() != package.read_bytes():
        raise RuntimeError(f"The Mover backup differs from the current pawn: {backup}")
    if not backup.exists():
        shutil.copy2(package, backup)

    if not editor.compile_blueprint(asset):
        raise RuntimeError(f"The Mover pawn failed to compile. Previous copy: {backup}")
    if not unreal.get_editor_subsystem(unreal.EditorAssetSubsystem).save_loaded_asset(asset, only_if_is_dirty=False):
        raise RuntimeError(f"Could not save the Mover pawn. Previous copy: {backup}")
    unreal.log("Native Mover input pipeline installed.")


if __name__ == "__main__":
    migrate()
