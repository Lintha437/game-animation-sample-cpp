"""Install native ragdoll state hooks on the locally imported Mover pawn."""

from pathlib import Path
import shutil

import unreal


ASSET = "/Game/Blueprints/SandboxCharacter_Mover_Ragdoll"
MOVER = "/Game/Blueprints/SandboxCharacter_Mover.SandboxCharacter_Mover_C"
FORWARDS = {
    "ProduceInput": ("BuildRagdollInputCommand", "InputCmdResult"),
    "Get_RagdollTransform": ("ResolveRagdollTransform", "ReturnValue"),
    "Get_RagdollTargetOrientation": ("ResolveRagdollFacing", "ReturnValue"),
    "Ragdoll_UpdateVelocities": ("UpdateRagdollVelocities", "None"),
    "Ragdoll_UpdateGravity": ("UpdateRagdollGravity", "None"),
    "CanDoCharacterInteraction": ("CanStartRagdollCharacterInteraction", "ReturnValue"),
    "CanBeInteractedWith": ("CanReceiveRagdollCharacterInteraction", "ReturnValue"),
    "CheckForInteractableCharacters": ("UpdateCharacterInteractionCandidates", "None"),
}


def migrate():
    descriptor = Path(unreal.Paths.get_project_file_path()).resolve()
    if descriptor.name != "GameAnimSampleCpp.uproject":
        raise RuntimeError("Run the migration in the native sample project.")
    editor = unreal.BlueprintEditorLibrary
    asset = unreal.load_asset(ASSET)
    parent = editor.get_blueprint_parent_class(asset) if asset else None
    native = editor.get_blueprint_parent_class(unreal.load_asset(MOVER.split(".")[0])) if parent else None
    if not parent or parent.get_path_name() != MOVER or not native or native.get_path_name() != (
        "/Script/GameAnimationSampleCpp.SampleMoverCharacter"
    ):
        raise RuntimeError("Apply the primary Mover migrations first.")

    def is_dirty():
        return any(package.get_name() == ASSET
                   for package in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages())

    if is_dirty():
        raise RuntimeError("The ragdoll pawn has unsaved changes.")
    graphs = {graph.get_name(): graph for graph in editor.list_graphs(asset)}
    if not FORWARDS.keys() <= graphs.keys():
        raise RuntimeError("The required ragdoll functions differ from the reference.")

    package = descriptor.parent / "Content/Blueprints/SandboxCharacter_Mover_Ragdoll.uasset"
    backup_root = descriptor.parent / "Saved/BlueprintMigration"
    # Install over the original package or over a pawn that already has the earlier state hooks.
    original_backup = backup_root / "RagdollState" / package.name
    backup = (backup_root / "RagdollInteractionCandidates" / package.name
              if original_backup.exists() else original_backup)
    migration = unreal.SampleBlueprintMigrationLibrary
    for name, (function, output) in FORWARDS.items():
        if not migration.replace_mover_input_function(asset, graphs[name], function, output):
            raise RuntimeError(f"Could not forward {name}. Discard unsaved changes.")
    if not is_dirty():
        unreal.log("Ragdoll state already migrated.")
        return

    backup.parent.mkdir(parents=True, exist_ok=True)
    if backup.exists() and backup.read_bytes() != package.read_bytes():
        raise RuntimeError(f"The ragdoll backup differs from the current pawn: {backup}")
    if not backup.exists():
        shutil.copy2(package, backup)
    if not editor.compile_blueprint(asset):
        raise RuntimeError(f"The ragdoll pawn failed to compile. Previous copy: {backup}")
    assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
    if not assets.save_loaded_asset(asset, only_if_is_dirty=False):
        raise RuntimeError(f"Could not save the ragdoll pawn. Previous copy: {backup}")
    unreal.log("Native ragdoll state installed.")


if __name__ == "__main__":
    migrate()
