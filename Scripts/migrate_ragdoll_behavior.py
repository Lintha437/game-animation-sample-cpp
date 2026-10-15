"""Forward ragdoll rolling and settled recovery to the native pawn."""

from pathlib import Path
import shutil

import unreal


ASSET = "/Game/Blueprints/SandboxCharacter_Mover_Ragdoll"
FUNCTIONS = {
    "Ragdoll_UpdateBehaviors": "UpdateRagdollBehavior",
    "Ragdoll_UpdateRollBehavior": "UpdateRagdollRollForce",
}


def migrate():
    descriptor = Path(unreal.Paths.get_project_file_path()).resolve()
    if descriptor.name != "GameAnimSampleCpp.uproject":
        raise RuntimeError("Run the migration in the native project.")
    editor = unreal.BlueprintEditorLibrary
    asset = unreal.load_asset(ASSET)
    graphs = {graph.get_name(): graph for graph in editor.list_graphs(asset)} if asset else {}

    def is_dirty():
        return any(package.get_name() == ASSET
                   for package in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages())

    if not (FUNCTIONS.keys() | {"EventGraph"}) <= graphs.keys() or is_dirty():
        raise RuntimeError("Load a clean ragdoll pawn with the original behavior graphs.")
    migration = unreal.SampleBlueprintMigrationLibrary
    if not migration.forward_isolated_event_override(asset, graphs["EventGraph"],
                                                     "NPC_GetupCheck", "ScheduleRagdollRecoveryCheck"):
        raise RuntimeError("Cannot forward the NPC recovery event. Discard unsaved changes.")
    for graph, function in FUNCTIONS.items():
        if not migration.replace_mover_input_function(asset, graphs[graph], function, "None"):
            raise RuntimeError(f"Cannot forward {graph}. Discard unsaved changes.")
    if not is_dirty():
        unreal.log("Ragdoll behavior already migrated.")
        return

    package = descriptor.parent / "Content" / (ASSET.removeprefix("/Game/") + ".uasset")
    backup = descriptor.parent / "Saved/BlueprintMigration/RagdollBehavior" / package.name
    if backup.exists() and backup.read_bytes() != package.read_bytes():
        raise RuntimeError(f"The ragdoll backup differs from the current asset: {backup}")
    backup.parent.mkdir(parents=True, exist_ok=True)
    if not backup.exists():
        shutil.copy2(package, backup)
    if not editor.compile_blueprint(asset):
        raise RuntimeError("The ragdoll pawn failed to compile.")
    assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
    if not assets.save_loaded_asset(asset, only_if_is_dirty=False):
        raise RuntimeError(f"Could not save the ragdoll pawn. Previous copy: {backup}")
    unreal.log("Native ragdoll behavior installed.")


if __name__ == "__main__":
    migrate()
