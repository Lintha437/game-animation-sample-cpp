"""Install native ragdoll entry and recovery on the locally imported pawn."""

from pathlib import Path
import shutil

import unreal


ASSET = "/Game/Blueprints/SandboxCharacter_Mover_Ragdoll"


def migrate():
    descriptor = Path(unreal.Paths.get_project_file_path()).resolve()
    if descriptor.name != "GameAnimSampleCpp.uproject":
        raise RuntimeError("Run the migration in the native sample project.")
    asset = unreal.load_asset(ASSET)
    if not asset:
        raise RuntimeError("Import the sample ragdoll pawn first.")
    editor = unreal.BlueprintEditorLibrary
    graphs = {graph.get_name(): graph for graph in editor.list_graphs(asset)}
    required = {"EventGraph", "On_MovementModeChanged_PostFinalize", "UpdateDefaultPhysicsProfileFromCvar"}
    if not required <= graphs.keys():
        raise RuntimeError("The imported ragdoll lifecycle differs from the reference.")

    def is_dirty():
        return any(package.get_name() == ASSET
                   for package in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages())

    if is_dirty():
        raise RuntimeError("The ragdoll pawn has unsaved changes.")
    migration = unreal.SampleBlueprintMigrationLibrary
    if not migration.replace_ragdoll_lifecycle(asset, graphs["EventGraph"], graphs["On_MovementModeChanged_PostFinalize"]):
        raise RuntimeError("Could not install the lifecycle hooks. Discard unsaved changes.")
    if not migration.replace_mover_input_function(
        asset, graphs["UpdateDefaultPhysicsProfileFromCvar"], "RefreshRagdollDefaultProfile", "None"
    ):
        raise RuntimeError("Could not forward the default profile update. Discard unsaved changes.")
    if not is_dirty():
        unreal.log("Ragdoll lifecycle already migrated.")
        return

    package = descriptor.parent / "Content/Blueprints/SandboxCharacter_Mover_Ragdoll.uasset"
    backup = descriptor.parent / "Saved/BlueprintMigration/RagdollLifecycle" / package.name
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
    unreal.log("Native ragdoll lifecycle installed.")


if __name__ == "__main__":
    migrate()
