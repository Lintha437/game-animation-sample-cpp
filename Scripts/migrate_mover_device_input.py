"""Bind Mover device handlers in C++ and retain the imported action assets."""

from pathlib import Path
import shutil

import unreal


ASSET = "/Game/Blueprints/SandboxCharacter_Mover"
PARENT = "/Script/GameAnimationSampleCpp.SampleMoverCharacter"
ACTIONS = ("IA_Look", "IA_Look_Gamepad", "IA_Crouch", "IA_Walk", "IA_Sprint", "IA_Strafe", "IA_Aim")
ACTION_PATHS = {f"/Game/Input/{name}.{name}" for name in ACTIONS}
TOGGLE = "use_native_mover_device_input"


def migrate():
    descriptor = Path(unreal.Paths.get_project_file_path()).resolve()
    if descriptor.name != "GameAnimSampleCpp.uproject":
        raise RuntimeError("Run the migration in the native project.")
    editor = unreal.BlueprintEditorLibrary
    asset = unreal.load_asset(ASSET)
    parent = editor.get_blueprint_parent_class(asset) if asset else None
    if not parent or parent.get_path_name() != PARENT:
        raise RuntimeError("Apply the primary Mover migrations first.")
    if any(package.get_name() == ASSET for package in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()):
        raise RuntimeError("The Mover pawn has unsaved changes.")
    graphs = {graph.get_name(): graph for graph in editor.list_graphs(asset)}
    if not {"EventGraph", "Get_MoveInput", "Get_RotationMode"} <= graphs.keys():
        raise RuntimeError("The imported Mover input graphs differ from the reference.")

    migration = unreal.SampleBlueprintMigrationLibrary
    events = graphs["EventGraph"]
    previous = set(migration.list_enhanced_input_actions(events))
    remaining = previous & ACTION_PATHS
    installed = unreal.get_default_object(editor.generated_class(asset)).get_editor_property(TOGGLE)
    if installed and not remaining:
        unreal.log("Mover device input already migrated.")
        return
    if installed or remaining != ACTION_PATHS:
        raise RuntimeError("The Mover pawn has a partial input migration.")

    package = descriptor.parent / "Content/Blueprints/SandboxCharacter_Mover.uasset"
    backup = descriptor.parent / "Saved/BlueprintMigration/MoverDeviceInput" / package.name
    backup.parent.mkdir(parents=True, exist_ok=True)
    if backup.exists() and backup.read_bytes() != package.read_bytes():
        raise RuntimeError(f"The Mover pawn differs from its backup: {backup}")
    if not backup.exists():
        shutil.copy2(package, backup)

    if not migration.remove_enhanced_input_actions(asset, events, sorted(ACTION_PATHS)):
        raise RuntimeError("Cannot isolate the Mover device events. Discard unsaved changes.")
    if not migration.replace_mover_input_function(asset, graphs["Get_MoveInput"], "ResolveMoverDirection", "ReturnValue"):
        raise RuntimeError("Cannot forward movement direction. Discard unsaved changes.")
    if not migration.replace_enum_query(asset, graphs["Get_RotationMode"], "ResolveMoverRotationMode"):
        raise RuntimeError("Cannot forward rotation mode. Discard unsaved changes.")
    if not editor.compile_blueprint(asset):
        raise RuntimeError(f"Mover pawn compile failed. Previous copy: {backup}")
    defaults = unreal.get_default_object(editor.generated_class(asset))
    defaults.set_editor_property(TOGGLE, True)
    assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
    if not assets.save_loaded_asset(asset, only_if_is_dirty=False):
        raise RuntimeError(f"Could not save the Mover pawn. Previous copy: {backup}")
    unreal.log("Native Mover device input installed.")


if __name__ == "__main__":
    migrate()
