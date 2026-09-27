"""Reparent the imported falling mode and slide transitions to their native implementations."""

from pathlib import Path
import shutil

import unreal


ROOT = "/Game/Blueprints/MovementModes"
NATIVE = "/Script/GameAnimationSampleCpp"
MODES = {
    "BP_MovementMode_Falling": ("FallingMode", "SampleMoverFallingMode", "GenerateMove"),
    "BP_MovementTransition_ToSlide": ("BaseMovementModeTransition", "SampleMoverEnterSlideTransition", "Evaluate"),
    "BP_MovementTransition_FromSlide": ("BaseMovementModeTransition", "SampleMoverExitSlideTransition", "Evaluate"),
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
        if function not in {graph.get_name() for graph in editor.list_graphs(asset)}:
            raise RuntimeError(f"The imported movement override is missing: {path}")
        package = descriptor.parent / f"Content/{path.removeprefix('/Game/')}.uasset"
        backup = descriptor.parent / "Saved/BlueprintMigration/MoverFallingAndTransitions" / package.name
        if backup.exists() and backup.read_bytes() != package.read_bytes():
            raise RuntimeError(f"Asset differs from its backup: {backup}")
        pending.append((asset, native, function, package, backup))

    for asset, native, function, package, backup in pending:
        backup.parent.mkdir(parents=True, exist_ok=True)
        if not backup.exists():
            shutil.copy2(package, backup)
        editor.remove_function_graph(asset, function)
        editor.reparent_blueprint(asset, native)
        if editor.get_blueprint_parent_class(asset) != native or not editor.compile_blueprint(asset):
            raise RuntimeError(f"Movement asset failed to compile: {asset.get_path_name()}")
        if not assets.save_loaded_asset(asset, only_if_is_dirty=False):
            raise RuntimeError(f"Could not save movement asset: {asset.get_path_name()}")
    unreal.log("Native Mover falling and slide transitions installed." if pending
               else "Mover falling and slide transitions already migrated.")


if __name__ == "__main__":
    migrate()
