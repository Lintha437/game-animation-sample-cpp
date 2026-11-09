"""Move navigation queries and callback registration behind the authored montage task."""

from hashlib import sha256
from pathlib import Path
import shutil

import unreal


ASSET = "/Game/Blueprints/SmartObjects/TasksAndConditions/STT_PlayAnimFromBestCost"
NATIVE = "/Script/GameAnimationSampleCpp.SampleMotionMatchApproachTask"
REFERENCE = "a1e6a1a8950da62ac32b5cdaa2f65ae042100b2d83fa9b933ffdfd152c0c104f"
FUNCTIONS = {
    "Setup Task": ("InitializeApproachTask", "None"),
    "NPC Approach Angle and Pathed Distance": ("ReadApproachDistance", "Distance"),
    "Get Actor Velocity": ("ReadApproachVelocity", "Velocity"),
}


def migrate():
    project = Path(unreal.Paths.get_project_file_path()).resolve()
    if project.name != "GameAnimSampleCpp.uproject":
        raise RuntimeError("Run this migration in the native sample project.")
    editor = unreal.BlueprintEditorLibrary
    asset = unreal.load_asset(ASSET)
    native = unreal.load_class(None, NATIVE)
    parent = editor.get_blueprint_parent_class(asset) if asset else None
    dirty = {package.get_name() for package in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()}
    if not parent or not native or ASSET in dirty:
        raise RuntimeError("The approach task or native class could not be loaded cleanly.")
    graphs = {graph.get_name(): graph for graph in editor.list_graphs(asset)}
    if not FUNCTIONS.keys() <= graphs.keys():
        raise RuntimeError("The approach task is missing a reference function.")
    already_reparented = parent == native
    if not already_reparented:
        package = project.parent / f"Content/{ASSET.removeprefix('/Game/')}.uasset"
        backup = project.parent / "Saved/BlueprintMigration/SmartObjectApproachTask" / package.name
        if (parent.get_path_name() != "/Script/StateTreeModule.StateTreeTaskBlueprintBase"
                or not editor.can_reparent_blueprint(asset, native)
                or sha256(package.read_bytes()).hexdigest() != REFERENCE):
            raise RuntimeError("The approach task differs from the reference.")
        backup.parent.mkdir(parents=True, exist_ok=True)
        if backup.exists() and sha256(backup.read_bytes()).hexdigest() != REFERENCE:
            raise RuntimeError(f"The approach task backup differs from the reference: {backup}")
        if not backup.exists():
            shutil.copy2(package, backup)
        editor.reparent_blueprint(asset, native)
        if editor.get_blueprint_parent_class(asset) != native:
            raise RuntimeError("Could not set the native approach task parent.")

    migration = unreal.SampleBlueprintMigrationLibrary
    for name, (function, output) in FUNCTIONS.items():
        if not migration.replace_mover_input_function(asset, graphs[name], function, output):
            raise RuntimeError(f"Could not forward {name}. Discard unsaved changes.")

    if already_reparented and not any(package.get_name() == ASSET
                                     for package in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()):
        unreal.log("Smart Object approach task already migrated.")
        return
    if not editor.compile_blueprint(asset):
        raise RuntimeError("The approach task failed to compile.")
    assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
    if not assets.save_loaded_asset(asset, only_if_is_dirty=False):
        raise RuntimeError("Could not save the native approach task.")
    unreal.log("Native Smart Object approach task installed.")


if __name__ == "__main__":
    migrate()
