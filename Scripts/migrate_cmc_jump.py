"""Connect the imported CMC jump and traversal action to the native pawn."""

from pathlib import Path
import shutil

import unreal


ASSET = "/Game/Blueprints/SandboxCharacter_CMC"
PARENT = "/Script/GameAnimationSampleCpp.SampleCmcCharacter"
ACTION = "/Game/Input/IA_Jump.IA_Jump"
TOGGLE = "use_native_cmc_jump_input"


def migrate():
    descriptor = Path(unreal.Paths.get_project_file_path()).resolve()
    if descriptor.name != "GameAnimSampleCpp.uproject":
        raise RuntimeError("Run the migration in the native project.")
    editor = unreal.BlueprintEditorLibrary
    asset = unreal.load_asset(ASSET)
    parent = editor.get_blueprint_parent_class(asset) if asset else None
    if not parent or parent.get_path_name() != PARENT:
        raise RuntimeError("Apply the primary CMC migrations first.")
    if any(package.get_name() == ASSET for package in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()):
        raise RuntimeError("The CMC pawn has unsaved changes.")
    graphs = {graph.get_name(): graph for graph in editor.list_graphs(asset)}
    if not {"EventGraph", "CanPerformJumpAction"} <= graphs.keys():
        raise RuntimeError("The imported CMC jump graphs differ from the reference.")

    migration = unreal.SampleBlueprintMigrationLibrary
    events = graphs["EventGraph"]
    previous = set(migration.list_enhanced_input_actions(events))
    enabled = unreal.get_default_object(editor.generated_class(asset)).get_editor_property(TOGGLE)
    if enabled and ACTION not in previous:
        unreal.log("CMC jump already migrated.")
        return
    if enabled or ACTION not in previous:
        raise RuntimeError("The CMC pawn has a partial jump migration.")

    package = descriptor.parent / "Content/Blueprints/SandboxCharacter_CMC.uasset"
    backup = descriptor.parent / "Saved/BlueprintMigration/CmcJump" / package.name
    backup.parent.mkdir(parents=True, exist_ok=True)
    if backup.exists() and backup.read_bytes() != package.read_bytes():
        raise RuntimeError(f"The CMC pawn differs from its backup: {backup}")
    if not backup.exists():
        shutil.copy2(package, backup)
    if not migration.remove_enhanced_input_actions(asset, events, [ACTION]):
        raise RuntimeError("Cannot isolate the CMC jump event. Discard unsaved changes.")
    if set(migration.list_enhanced_input_actions(events)) != previous - {ACTION}:
        raise RuntimeError("Unexpected changes to CMC bindings. Discard unsaved changes.")
    if not migration.replace_mover_input_function(asset, graphs["CanPerformJumpAction"], "CanStartCmcJump", "ReturnValue"):
        raise RuntimeError("Could not forward the CMC jump gate. Discard unsaved changes.")
    if not editor.compile_blueprint(asset):
        raise RuntimeError(f"CMC pawn compile failed. Previous copy: {backup}")
    unreal.get_default_object(editor.generated_class(asset)).set_editor_property(TOGGLE, True)
    assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
    if not assets.save_loaded_asset(asset, only_if_is_dirty=False):
        raise RuntimeError(f"Could not save the CMC pawn. Previous copy: {backup}")
    unreal.log("Native CMC jump installed.")


if __name__ == "__main__":
    migrate()
