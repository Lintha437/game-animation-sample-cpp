"""Install the native CMC movement implementation in the local reference character."""

from hashlib import sha256
from pathlib import Path
import shutil

import unreal


ASSET = "/Game/Blueprints/SandboxCharacter_CMC"
NATIVE_PARENT = "/Script/GameAnimationSampleCpp.SampleCmcCharacter"
REFERENCE_HASH = "26309aa15138a0ab5091e4f52fc26c7b95cc1b5cb8b3337a40eb4928a9ea83af"
STAGES = {
    "CrouchSpeed": frozenset({"CalculateMaxCrouchSpeed"}),
    "Movement": frozenset({
        "GetMovementInputScaleValue", "CanSprint", "UpdateRotation_PreCMC",
        "CalculateMaxAcceleration", "CalculateGroundFriction", "CalculateMaxSpeed",
    }),
    "Gait": frozenset({
        "GetDesiredGait", "UpdateMovement_PreCMC", "HasMovementInputVector",
        "CalculateBrakingDeceleration", "CalculateBrakingFriction",
    }),
}
REMOVABLE_GRAPHS = frozenset().union(*STAGES.values())
STARTING_STATES = {
    REMOVABLE_GRAPHS: "Reference",
    STAGES["Gait"]: "Movement",
}


def authored_defaults(default_object):
    vectors = (default_object.get_editor_property(name)
               for name in ("walk_speeds", "run_speeds", "sprint_speeds", "crouch_speeds"))
    speeds = tuple((value.x, value.y, value.z) for value in vectors)
    curve = default_object.get_editor_property("strafe_speed_map_curve")
    return (
        speeds,
        curve.get_path_name() if curve else None,
        default_object.get_editor_property("movement_stick_mode"),
        default_object.get_editor_property("AnalogWalk/RunThreshold"),
        default_object.get_editor_property("gait"),
    )


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
    if "CanPerformJumpAction" not in graphs or "EventGraph" not in graphs:
        raise RuntimeError("The authored montage gate or event graph is missing.")

    remaining = REMOVABLE_GRAPHS.intersection(graphs)
    old_parent = editor.get_blueprint_parent_class(asset)
    if not remaining and old_parent == parent:
        unreal.log("CMC gait already migrated.")
        return

    package = descriptor.parent / "Content/Blueprints/SandboxCharacter_CMC.uasset"
    stage = STARTING_STATES.get(remaining)
    if stage == "Reference":
        if (
            old_parent != unreal.Character.static_class()
            or not editor.can_reparent_blueprint(asset, parent)
            or sha256(package.read_bytes()).hexdigest() != REFERENCE_HASH
        ):
            raise RuntimeError("The stock CMC package differs from the supported reference.")
    elif stage is None or old_parent != parent:
        raise RuntimeError("Unexpected CMC character graph layout or parent.")

    backup = descriptor.parent / "Saved/BlueprintMigration/CmcGait" / stage / package.name
    backup.parent.mkdir(parents=True, exist_ok=True)
    if backup.exists() and backup.read_bytes() != package.read_bytes():
        raise RuntimeError(f"Existing backup differs from the package: {backup}")
    if not backup.exists():
        shutil.copy2(package, backup)

    before = authored_defaults(unreal.get_default_object(editor.generated_class(asset)))
    if not unreal.SampleBlueprintMigrationLibrary.retarget_self_call(
        graphs["EventGraph"], "UpdateMovement_PreCMC", "UpdateMovementBeforeSimulation"
    ):
        raise RuntimeError(f"Expected one pre-CMC movement call. Previous copy: {backup}")
    for name in sorted(remaining):
        editor.remove_graph(asset, graphs[name])
    if old_parent != parent:
        editor.reparent_blueprint(asset, parent)
    if not editor.compile_blueprint(asset):
        raise RuntimeError(f"CMC compilation failed. Previous copy: {backup}")
    if authored_defaults(unreal.get_default_object(editor.generated_class(asset))) != before:
        raise RuntimeError(f"Authored CMC defaults changed. Previous copy: {backup}")
    if not unreal.get_editor_subsystem(unreal.EditorAssetSubsystem).save_loaded_asset(asset, only_if_is_dirty=False):
        raise RuntimeError(f"Failed to save the CMC character. Previous copy: {backup}")
    unreal.log(f"Native CMC gait installed from {stage}.")


if __name__ == "__main__":
    migrate()
