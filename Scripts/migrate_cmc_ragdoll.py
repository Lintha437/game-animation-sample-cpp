"""Connect the imported CMC pawn's ragdoll and ledge hooks to native code."""

from pathlib import Path
import shutil

import unreal


ASSET = "/Game/Blueprints/SandboxCharacter_CMC"
PARENT = "/Script/GameAnimationSampleCpp.SampleCmcCharacter"
FORWARDS = {
    "Ragdoll_Start": "BeginCmcRagdoll",
    "Ragdoll_End": "EndCmcRagdoll",
}


def migrate():
    descriptor = Path(unreal.Paths.get_project_file_path()).resolve()
    if descriptor.name != "GameAnimSampleCpp.uproject":
        raise RuntimeError("Run the migration in the native project.")
    editor = unreal.BlueprintEditorLibrary
    asset = unreal.load_asset(ASSET)
    parent = editor.get_blueprint_parent_class(asset) if asset else None
    if not parent or parent.get_path_name() != PARENT:
        raise RuntimeError("Apply the primary CMC migrations first.")

    def is_dirty():
        return any(package.get_name() == ASSET
                   for package in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages())

    if is_dirty():
        raise RuntimeError("The CMC pawn has unsaved changes.")
    graphs = {graph.get_name(): graph for graph in editor.list_graphs(asset)}
    if not (FORWARDS.keys() | {"EventGraph"}) <= graphs.keys():
        raise RuntimeError("The imported CMC ragdoll graphs differ from the reference.")

    migration = unreal.SampleBlueprintMigrationLibrary
    for name, native in FORWARDS.items():
        if not migration.replace_mover_input_function(asset, graphs[name], native, "None"):
            raise RuntimeError(f"Cannot forward {name}. Discard unsaved changes.")
    if not migration.remove_isolated_event_override(asset, graphs["EventGraph"], "OnWalkingOffLedge"):
        raise RuntimeError("Cannot isolate the ledge event. Discard unsaved changes.")
    if not is_dirty():
        unreal.log("CMC ragdoll already migrated.")
        return

    package = descriptor.parent / "Content/Blueprints/SandboxCharacter_CMC.uasset"
    backup = descriptor.parent / "Saved/BlueprintMigration/CmcRagdoll" / package.name
    backup.parent.mkdir(parents=True, exist_ok=True)
    if backup.exists() and backup.read_bytes() != package.read_bytes():
        raise RuntimeError(f"The CMC pawn differs from its backup: {backup}")
    if not backup.exists():
        shutil.copy2(package, backup)
    if not editor.compile_blueprint(asset):
        raise RuntimeError(f"CMC pawn compile failed. Previous copy: {backup}")
    assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
    if not assets.save_loaded_asset(asset, only_if_is_dirty=False):
        raise RuntimeError(f"Could not save the CMC pawn. Previous copy: {backup}")
    unreal.log("Native CMC ragdoll installed.")


if __name__ == "__main__":
    migrate()
