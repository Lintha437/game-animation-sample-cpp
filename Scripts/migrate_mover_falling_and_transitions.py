"""Reparent the imported air and ground modes and slide transitions to native classes."""

from pathlib import Path
import shutil

import unreal


ROOT = "/Game/Blueprints/MovementModes"
NATIVE = "/Script/GameAnimationSampleCpp"
MODES = {
    "BP_MovementMode_Falling": ("FallingMode", "SampleMoverFallingMode", "GenerateMove"),
    "BP_MovementTransition_ToSlide": ("BaseMovementModeTransition", "SampleMoverEnterSlideTransition", "Evaluate"),
    "BP_MovementTransition_FromSlide": ("BaseMovementModeTransition", "SampleMoverExitSlideTransition", "Evaluate"),
    "BP_MovementMode_Walking": ("SmoothWalkingMode", "SampleMoverWalkingMode", "GenerateWalkMove"),
    "BP_MovementMode_Slide": ("SmoothWalkingMode", "SampleMoverSlidingMode", "GenerateWalkMove"),
}


def migrate():
    descriptor = Path(unreal.Paths.get_project_file_path()).resolve()
    if descriptor.name != "GameAnimSampleCpp.uproject":
        raise RuntimeError("Run the migration in the native project.")
    editor = unreal.BlueprintEditorLibrary
    assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
    dirty = {item.get_name() for item in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()}
    pending = []
    for name, (stock, class_name, function) in MODES.items():
        path = f"{ROOT}/{name}"
        asset = unreal.load_asset(path)
        native = unreal.load_class(None, f"{NATIVE}.{class_name}")
        parent = editor.get_blueprint_parent_class(asset) if asset else None
        if not parent or not native:
            raise RuntimeError(f"Cannot load the asset or native parent: {path}")
        if parent == native:
            continue
        if (parent.get_path_name() != f"/Script/Mover.{stock}" or path in dirty
                or not editor.can_reparent_blueprint(asset, native)):
            raise RuntimeError(f"Asset is dirty or its parent differs from the reference: {path}")
        graphs = {graph.get_name() for graph in editor.list_graphs(asset)}
        event = editor.find_event_graph(asset) if stock == "SmoothWalkingMode" else None
        if function not in graphs or (stock == "SmoothWalkingMode" and not event):
            raise RuntimeError(f"The imported movement override is missing: {path}")
        package = descriptor.parent / f"Content/{path.removeprefix('/Game/')}.uasset"
        group = "MoverGroundModes" if event else "MoverFallingAndTransitions"
        backup = descriptor.parent / "Saved/BlueprintMigration" / group / package.name
        if backup.exists() and backup.read_bytes() != package.read_bytes():
            raise RuntimeError(f"Asset differs from its backup: {backup}")
        pending.append((asset, native, function, event, package, backup))

    for asset, native, function, event, package, backup in pending:
        backup.parent.mkdir(parents=True, exist_ok=True)
        if not backup.exists():
            shutil.copy2(package, backup)
        if event and not unreal.SampleBlueprintMigrationLibrary.remove_isolated_event_override(asset, event, "K2_OnActivated"):
            raise RuntimeError(f"Cannot isolate the mode activation graph: {asset.get_path_name()}")
        editor.remove_function_graph(asset, function)
        editor.reparent_blueprint(asset, native)
        if editor.get_blueprint_parent_class(asset) != native or not editor.compile_blueprint(asset):
            raise RuntimeError(f"Movement asset failed to compile: {asset.get_path_name()}")
        if not assets.save_loaded_asset(asset, only_if_is_dirty=False):
            raise RuntimeError(f"Could not save movement asset: {asset.get_path_name()}")
    unreal.log("Native Mover modes and slide transitions installed." if pending
               else "Mover modes and slide transitions already migrated.")


if __name__ == "__main__":
    migrate()
