"""Keep the StateTree bindings and transfer slot search, claim, and release to C++."""

from pathlib import Path
import shutil

import unreal


ROOT = "/Game/Blueprints/SmartObjects/TasksAndConditions"
NATIVE_PACKAGE = "/Script/GameAnimationSampleCpp"
STOCK_PARENT = "/Script/StateTreeModule.StateTreeTaskBlueprintBase"
TASKS = {
    "STT_FindSmartObject": "SampleFindSmartObjectTask",
    "STT_ClaimSlot": "SampleClaimSmartObjectTask",
    "STT_ClearHandle": "SampleReleaseSmartObjectTask",
}


def migrate():
    descriptor = Path(unreal.Paths.get_project_file_path()).resolve()
    if descriptor.name != "GameAnimSampleCpp.uproject":
        raise RuntimeError("Run the migration in the native project.")
    editor = unreal.BlueprintEditorLibrary
    assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
    dirty = {package.get_name() for package in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()}
    pending = []
    for name, class_name in TASKS.items():
        asset_path = f"{ROOT}/{name}"
        asset = unreal.load_asset(asset_path)
        native = unreal.load_class(None, f"{NATIVE_PACKAGE}.{class_name}")
        current = editor.get_blueprint_parent_class(asset) if asset else None
        if not current or not native:
            raise RuntimeError(f"Cannot load the task or native parent: {asset_path}")
        if current == native:
            continue
        if current.get_path_name() != STOCK_PARENT or asset_path in dirty or not editor.can_reparent_blueprint(asset, native):
            raise RuntimeError(f"Task is dirty or its parent differs from the reference: {asset_path}")
        graph = next((graph for graph in editor.list_graphs(asset) if graph.get_name() == "EventGraph"), None)
        if not graph:
            raise RuntimeError(f"No event graph for {asset_path}")
        package = descriptor.parent / f"Content/{asset_path.removeprefix('/Game/')}.uasset"
        backup = descriptor.parent / "Saved/BlueprintMigration/SmartObjectSlots" / package.name
        if backup.exists() and backup.read_bytes() != package.read_bytes():
            raise RuntimeError(f"Task backup differs from the current package: {backup}")
        pending.append((asset, graph, native, package, backup))

    for asset, graph, native, package, backup in pending:
        backup.parent.mkdir(parents=True, exist_ok=True)
        if not backup.exists():
            shutil.copy2(package, backup)
        if not unreal.SampleBlueprintMigrationLibrary.remove_isolated_event_override(asset, graph, "ReceiveLatentEnterState"):
            raise RuntimeError(f"Task has a shared entry graph: {asset.get_path_name()}")
        editor.reparent_blueprint(asset, native)
        if editor.get_blueprint_parent_class(asset) != native or not editor.compile_blueprint(asset):
            raise RuntimeError(f"Task failed to compile: {asset.get_path_name()}")
        if not assets.save_loaded_asset(asset, only_if_is_dirty=False):
            raise RuntimeError(f"Could not save task: {asset.get_path_name()}")
    unreal.log("Native Smart Object slots installed." if pending else "Smart Object slots already migrated.")


if __name__ == "__main__":
    migrate()
