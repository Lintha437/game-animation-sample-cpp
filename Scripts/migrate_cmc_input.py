"""Replace the local reference pawn's movement, look, and crouch input events."""

from pathlib import Path
import shutil

import unreal


ASSET = "/Game/Blueprints/SandboxCharacter_CMC"
ACTION_NAMES = ("IA_Move", "IA_Move_WorldSpace", "IA_Look", "IA_Look_Gamepad", "IA_Crouch")
ACTION_PATHS = {f"/Game/Input/{name}.{name}" for name in ACTION_NAMES}
TOGGLE = "use_native_input_handlers"


def migrate():
    descriptor = Path(unreal.Paths.get_project_file_path()).resolve()
    if descriptor.name != "GameAnimSampleCpp.uproject":
        raise RuntimeError("Run the migration in the native sample project.")
    if any(package.get_name() == ASSET for package in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()):
        raise RuntimeError("The CMC character has unsaved changes.")

    editor = unreal.BlueprintEditorLibrary
    migration = unreal.SampleBlueprintMigrationLibrary
    asset = unreal.load_asset(ASSET)
    parent = unreal.load_class(None, "/Script/GameAnimationSampleCpp.SampleCmcCharacter")
    if not asset or not parent or editor.get_blueprint_parent_class(asset) != parent:
        raise RuntimeError("Build the Editor target and run the CMC gait migration first.")
    graphs = {graph.get_name(): graph for graph in editor.list_graphs(asset)}
    if "GetDesiredGait" in graphs or "CanPerformJumpAction" not in graphs or "EventGraph" not in graphs:
        raise RuntimeError("Unexpected CMC graph layout. Run the CMC gait migration first.")

    graph = graphs["EventGraph"]
    defaults = unreal.get_default_object(editor.generated_class(asset))
    previous_actions = set(migration.list_enhanced_input_actions(graph))
    remaining = ACTION_PATHS.intersection(previous_actions)
    installed = defaults.get_editor_property(TOGGLE)
    if not remaining and installed:
        unreal.log("CMC input already migrated.")
        return
    if installed or remaining != ACTION_PATHS:
        raise RuntimeError("The CMC character contains a partial input migration.")

    package = descriptor.parent / "Content/Blueprints/SandboxCharacter_CMC.uasset"
    backup = descriptor.parent / "Saved/BlueprintMigration/CmcInput" / package.name
    backup.parent.mkdir(parents=True, exist_ok=True)
    if backup.exists() and backup.read_bytes() != package.read_bytes():
        raise RuntimeError(f"The existing CMC input backup differs from the current package: {backup}")
    if not backup.exists():
        shutil.copy2(package, backup)

    if not migration.remove_enhanced_input_actions(asset, graph, sorted(ACTION_PATHS)):
        raise RuntimeError(f"The CMC input chains could not be isolated. Previous copy: {backup}")
    if not editor.compile_blueprint(asset):
        raise RuntimeError(f"The CMC character failed to compile. Previous copy: {backup}")
    if set(migration.list_enhanced_input_actions(graph)) != previous_actions - ACTION_PATHS:
        raise RuntimeError(f"Unexpected remaining action bindings. Previous copy: {backup}")
    defaults = unreal.get_default_object(editor.generated_class(asset))
    defaults.set_editor_property(TOGGLE, True)
    if not unreal.get_editor_subsystem(unreal.EditorAssetSubsystem).save_loaded_asset(asset, only_if_is_dirty=False):
        raise RuntimeError(f"Failed to save the CMC character. Previous copy: {backup}")
    unreal.log("Native CMC input installed.")


if __name__ == "__main__":
    migrate()
