"""Move the reference CMC character's crouch speed calculation to C++."""

from hashlib import sha256
from pathlib import Path
import shutil

import unreal


ASSET = "/Game/Blueprints/SandboxCharacter_CMC"
NATIVE_PARENT = "/Script/GameAnimationSampleCpp.SampleCmcCharacter"
ORIGINAL_SHA256 = "26309aa15138a0ab5091e4f52fc26c7b95cc1b5cb8b3337a40eb4928a9ea83af"
CROUCH_SPEED_GRAPH = "CalculateMaxCrouchSpeed"
JUMP_GATE_GRAPH = "CanPerformJumpAction"


def movement_defaults(default_object):
    speeds = default_object.get_editor_property("crouch_speeds")
    curve = default_object.get_editor_property("strafe_speed_map_curve")
    return speeds.x, speeds.y, speeds.z, curve.get_path_name() if curve else None


def migrate():
    descriptor = Path(unreal.Paths.get_project_file_path()).resolve()
    if descriptor.name != "GameAnimSampleCpp.uproject":
        raise RuntimeError("Run the migration in the native sample project.")
    if any(package.get_name() == ASSET for package in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()):
        raise RuntimeError("The CMC character has unsaved changes.")

    editor = unreal.BlueprintEditorLibrary
    asset = unreal.load_asset(ASSET)
    parent = unreal.load_class(None, NATIVE_PARENT)
    if not asset or not parent:
        raise RuntimeError("Import the reference content and build the native Editor target.")

    graphs = {graph.get_name(): graph for graph in editor.list_graphs(asset)}
    old_parent = editor.get_blueprint_parent_class(asset)
    if old_parent == parent and CROUCH_SPEED_GRAPH not in graphs and JUMP_GATE_GRAPH in graphs:
        unreal.log("CMC crouch speed already migrated.")
        return

    package = descriptor.parent / "Content/Blueprints/SandboxCharacter_CMC.uasset"
    if (
        old_parent != unreal.Character.static_class()
        or CROUCH_SPEED_GRAPH not in graphs
        or JUMP_GATE_GRAPH not in graphs
        or sha256(package.read_bytes()).hexdigest() != ORIGINAL_SHA256
        or not editor.can_reparent_blueprint(asset, parent)
    ):
        raise RuntimeError("Unexpected CMC character package or parent.")

    backup = descriptor.parent / "Saved/BlueprintMigration/CmcCrouchSpeed" / package.name
    backup.parent.mkdir(parents=True, exist_ok=True)
    if backup.exists() and sha256(backup.read_bytes()).hexdigest() != ORIGINAL_SHA256:
        raise RuntimeError("The existing CMC character backup differs from the reference.")
    if not backup.exists():
        shutil.copy2(package, backup)

    original = unreal.get_default_object(editor.generated_class(asset))
    original_defaults = movement_defaults(original)
    editor.remove_graph(asset, graphs[CROUCH_SPEED_GRAPH])
    editor.reparent_blueprint(asset, parent)
    if not editor.compile_blueprint(asset):
        raise RuntimeError(f"CMC character compilation failed. Original copy: {backup}")

    updated = unreal.get_default_object(editor.generated_class(asset))
    if movement_defaults(updated) != original_defaults:
        raise RuntimeError(f"CMC movement defaults changed. Original copy: {backup}")
    if not unreal.get_editor_subsystem(unreal.EditorAssetSubsystem).save_loaded_asset(asset, only_if_is_dirty=False):
        raise RuntimeError(f"Failed to save the CMC character. Original copy: {backup}")
    unreal.log("Native CMC crouch speed installed.")


if __name__ == "__main__":
    migrate()
