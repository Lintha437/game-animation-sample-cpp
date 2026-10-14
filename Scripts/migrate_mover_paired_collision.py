"""Move Mover paired-action collision ownership to the native pawn."""

from pathlib import Path
import shutil

import unreal


ASSET = "/Game/Blueprints/SandboxCharacter_Mover_Ragdoll"


def migrate():
    descriptor = Path(unreal.Paths.get_project_file_path()).resolve()
    if descriptor.name != "GameAnimSampleCpp.uproject":
        raise RuntimeError("Run the migration in the native project.")
    editor = unreal.BlueprintEditorLibrary
    asset = unreal.load_asset(ASSET)
    events = editor.find_event_graph(asset) if asset else None
    match = editor.find_graph(asset, "Try_MultiCharacterInteraction") if asset else None

    def is_dirty():
        return any(package.get_name() == ASSET
                   for package in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages())

    if not events or not match or is_dirty():
        raise RuntimeError("Load a clean ragdoll pawn with the original interaction graphs.")
    if not unreal.SampleBlueprintMigrationLibrary.replace_paired_interaction_collision(asset, match, events):
        raise RuntimeError("The paired collision or montage cleanup graph differs from the reference.")
    if not is_dirty():
        unreal.log("Mover paired collision already migrated.")
        return

    package = descriptor.parent / "Content" / (ASSET.removeprefix("/Game/") + ".uasset")
    backup = descriptor.parent / "Saved/BlueprintMigration/MoverPairedCollision" / package.name
    if backup.exists() and backup.read_bytes() != package.read_bytes():
        raise RuntimeError(f"The paired collision backup differs from the current asset: {backup}")
    backup.parent.mkdir(parents=True, exist_ok=True)
    if not backup.exists():
        shutil.copy2(package, backup)
    if not editor.compile_blueprint(asset):
        raise RuntimeError("The ragdoll pawn failed to compile.")
    assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
    if not assets.save_loaded_asset(asset, only_if_is_dirty=False):
        raise RuntimeError(f"Could not save the ragdoll pawn. Previous copy: {backup}")
    unreal.log("Native Mover paired collision installed.")


if __name__ == "__main__":
    migrate()
