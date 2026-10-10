"""Install native pointer bindings and gizmo tracking on the migrated Look At POI zone."""

from pathlib import Path
import shutil

import unreal


ASSET = "/Game/IsolatedExamples/LookAtPOI/TestLevelContent/Props/BP_LookAtPOI_BorderObj"


def migrate():
    descriptor = Path(unreal.Paths.get_project_file_path()).resolve()
    if descriptor.name != "GameAnimSampleCpp.uproject":
        raise RuntimeError("Run the migration in the native project.")
    editor = unreal.BlueprintEditorLibrary
    asset = unreal.load_asset(ASSET)
    native = unreal.load_class(None, "/Script/GameAnimationSampleCpp.SampleLookAtZone")
    dirty = {item.get_name() for item in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()}
    if not asset or not native or ASSET in dirty or editor.get_blueprint_parent_class(asset) != native:
        raise RuntimeError("Run migrate_look_at_zones.py on a clean Look At zone first.")
    previous_position = editor.find_graph(asset, "Get Gizmo Pos")
    if not previous_position:
        unreal.log("Look At pointer already migrated.")
        return

    graph = editor.find_event_graph(asset)
    if not graph:
        raise RuntimeError("The Look At zone event graph is missing.")
    package = descriptor.parent / "Content" / (ASSET.removeprefix("/Game/") + ".uasset")
    backup = descriptor.parent / "Saved/BlueprintMigration/LookAtZonePointer" / package.name
    if backup.exists() and backup.read_bytes() != package.read_bytes():
        raise RuntimeError(f"The Look At zone backup differs from the current asset: {backup}")
    backup.parent.mkdir(parents=True, exist_ok=True)
    if not backup.exists():
        shutil.copy2(package, backup)

    migration = unreal.SampleBlueprintMigrationLibrary
    for event in ("ReceiveBeginPlay", "ReceiveTick"):
        if not migration.remove_isolated_event_override(asset, graph, event):
            raise RuntimeError(f"Cannot remove {event}. Discard unsaved changes.")
    if not migration.remove_custom_events(asset, graph, ["On Begin Mouse Over", "On End Mouse Over", "Set Color"]):
        raise RuntimeError("Cannot remove the former hover and color handlers. Discard unsaved changes.")
    if not migration.remove_declared_variable(asset, "All Bodred Objects"):
        raise RuntimeError("Cannot remove the former zone cache. Discard unsaved changes.")
    editor.remove_graph(asset, previous_position)
    if not editor.compile_blueprint(asset):
        raise RuntimeError("The Look At zone failed to compile.")
    assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
    if not assets.save_loaded_asset(asset, only_if_is_dirty=False):
        raise RuntimeError(f"Could not save the Look At zone. Previous copy: {backup}")
    unreal.log("Native Look At pointer installed.")


if __name__ == "__main__":
    migrate()
