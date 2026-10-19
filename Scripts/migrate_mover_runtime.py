"""Forward the primary Mover pawn's runtime startup and tick to C++."""

from pathlib import Path
import shutil

import unreal


ASSET = "/Game/Blueprints/SandboxCharacter_Mover"
EVENTS = {"ReceiveBeginPlay": "InitializeMoverRuntime", "ReceiveTick": "UpdateMoverRuntime"}


def migrate():
    descriptor = Path(unreal.Paths.get_project_file_path()).resolve()
    if descriptor.name != "GameAnimSampleCpp.uproject":
        raise RuntimeError("Run the migration in the native project.")
    editor = unreal.BlueprintEditorLibrary
    asset = unreal.load_asset(ASSET)
    graph = editor.find_event_graph(asset) if asset else None
    parent = editor.get_blueprint_parent_class(asset) if asset else None

    def is_dirty():
        return any(package.get_name() == ASSET
                   for package in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages())

    if not graph or not parent or parent.get_path_name() != "/Script/GameAnimationSampleCpp.SampleMoverCharacter" or is_dirty():
        raise RuntimeError("Load the clean Mover pawn after its native state and input migrations.")
    migration = unreal.SampleBlueprintMigrationLibrary
    for event, function in EVENTS.items():
        if not migration.forward_isolated_event_override(asset, graph, event, function):
            raise RuntimeError(f"Cannot forward {event}. Discard unsaved changes.")
    if not is_dirty():
        unreal.log("Mover runtime already migrated.")
        return

    package = descriptor.parent / "Content" / (ASSET.removeprefix("/Game/") + ".uasset")
    backup = descriptor.parent / "Saved/BlueprintMigration/MoverRuntime" / package.name
    if backup.exists() and backup.read_bytes() != package.read_bytes():
        raise RuntimeError(f"The Mover backup differs from the current asset: {backup}")
    backup.parent.mkdir(parents=True, exist_ok=True)
    if not backup.exists():
        shutil.copy2(package, backup)
    if not editor.compile_blueprint(asset):
        raise RuntimeError("The Mover pawn failed to compile.")
    assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
    if not assets.save_loaded_asset(asset, only_if_is_dirty=False):
        raise RuntimeError(f"Could not save the Mover pawn. Previous copy: {backup}")
    unreal.log("Native Mover runtime installed.")


if __name__ == "__main__":
    migrate()
