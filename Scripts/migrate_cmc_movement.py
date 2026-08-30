"""Switch the local CMC movement functions to their inherited native implementations."""

from pathlib import Path
import shutil

import unreal

ASSET = "/Game/Blueprints/SandboxCharacter_CMC"
NATIVE_PARENT = "/Script/GameAnimationSampleCpp.SampleCmcCharacter"
CROUCH_SPEED_GRAPH = "CalculateMaxCrouchSpeed"
NATIVE_GRAPHS = {
    "GetMovementInputScaleValue",
    "CanSprint",
    "UpdateRotation_PreCMC",
    "CalculateMaxAcceleration",
    "CalculateGroundFriction",
    "CalculateMaxSpeed",
}
KEEP_GRAPHS = {"UpdateMovement_PreCMC", "GetDesiredGait", "CanPerformJumpAction"}


def authored_defaults(default_object):
    fields = ("walk_speeds", "run_speeds", "sprint_speeds", "crouch_speeds")
    vectors = (default_object.get_editor_property(name) for name in fields)
    speeds = tuple((vector.x, vector.y, vector.z) for vector in vectors)
    curve = default_object.get_editor_property("strafe_speed_map_curve")
    return speeds, curve.get_path_name() if curve else None, default_object.get_editor_property("movement_stick_mode")


def migrate():
    descriptor = Path(unreal.Paths.get_project_file_path()).resolve()
    if descriptor.name != "GameAnimSampleCpp.uproject":
        raise RuntimeError("Run the migration in the native sample project.")
    if any(package.get_name() == ASSET for package in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()):
        raise RuntimeError("The CMC character has unsaved changes.")

    editor = unreal.BlueprintEditorLibrary
    asset = unreal.load_asset(ASSET)
    native_parent = unreal.load_class(None, NATIVE_PARENT)
    if not asset or not native_parent or editor.get_blueprint_parent_class(asset) != native_parent:
        raise RuntimeError("Build the Editor target and run the CMC crouch speed migration first.")

    graphs = {graph.get_name(): graph for graph in editor.list_graphs(asset)}
    if not KEEP_GRAPHS.issubset(graphs) or CROUCH_SPEED_GRAPH in graphs:
        raise RuntimeError("Unexpected CMC graph layout.")
    remaining = NATIVE_GRAPHS.intersection(graphs)
    if not remaining:
        unreal.log("CMC movement already migrated.")
        return
    if remaining != NATIVE_GRAPHS:
        raise RuntimeError("The character contains only part of the original movement graphs.")

    package = descriptor.parent / "Content/Blueprints/SandboxCharacter_CMC.uasset"
    backup = descriptor.parent / "Saved/BlueprintMigration/CmcMovement" / package.name
    backup.parent.mkdir(parents=True, exist_ok=True)
    if backup.exists() and backup.read_bytes() != package.read_bytes():
        raise RuntimeError(f"The existing CMC movement backup differs from the current package: {backup}")
    if not backup.exists():
        shutil.copy2(package, backup)

    before = authored_defaults(unreal.get_default_object(editor.generated_class(asset)))
    for name in sorted(NATIVE_GRAPHS):
        editor.remove_graph(asset, graphs[name])
    if not editor.compile_blueprint(asset):
        raise RuntimeError(f"CMC movement compilation failed. Previous copy: {backup}")
    after = authored_defaults(unreal.get_default_object(editor.generated_class(asset)))
    if after != before:
        raise RuntimeError(f"Authored CMC defaults changed. Previous copy: {backup}")
    if not unreal.get_editor_subsystem(unreal.EditorAssetSubsystem).save_loaded_asset(asset, only_if_is_dirty=False):
        raise RuntimeError(f"Failed to save the CMC character. Previous copy: {backup}")
    unreal.log("Native CMC movement installed.")


if __name__ == "__main__":
    migrate()
