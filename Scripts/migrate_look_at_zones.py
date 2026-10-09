"""Move Look At POI zone selection and gizmo ownership to native code."""

from hashlib import sha256
from pathlib import Path
import shutil

import unreal


ASSET = "/Game/IsolatedExamples/LookAtPOI/TestLevelContent/Props/BP_LookAtPOI_BorderObj"
REFERENCE = "a180fd7a23a4e40ea8a2d7fe33b70c47b630a4cba787e0bec7ce6ba991e9a623"
EVENTS = {"On Mouse Clicked": "SelectLookAtZone", "Deselect Zone": "DeselectLookAtZone"}


def migrate():
    descriptor = Path(unreal.Paths.get_project_file_path()).resolve()
    if descriptor.name != "GameAnimSampleCpp.uproject":
        raise RuntimeError("Run the migration in the native project.")
    editor = unreal.BlueprintEditorLibrary
    asset = unreal.load_asset(ASSET)
    native = unreal.load_class(None, "/Script/GameAnimationSampleCpp.SampleLookAtZone")
    dirty = {item.get_name() for item in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()}
    if not asset or not native or ASSET in dirty:
        raise RuntimeError("Cannot load a clean Look At zone and its native parent.")
    parent = editor.get_blueprint_parent_class(asset)
    if parent == native:
        unreal.log("Look At zone already migrated.")
        return

    package = descriptor.parent / "Content" / (ASSET.removeprefix("/Game/") + ".uasset")
    graph = editor.find_event_graph(asset)
    if (parent != unreal.Actor.static_class() or not graph or not editor.can_reparent_blueprint(asset, native)
            or sha256(package.read_bytes()).hexdigest() != REFERENCE):
        raise RuntimeError("The Look At zone differs from the reference.")
    backup = descriptor.parent / "Saved/BlueprintMigration/LookAtZones" / package.name
    if backup.exists() and sha256(backup.read_bytes()).hexdigest() != REFERENCE:
        raise RuntimeError(f"The Look At zone backup differs from the reference: {backup}")
    backup.parent.mkdir(parents=True, exist_ok=True)
    if not backup.exists():
        shutil.copy2(package, backup)

    editor.reparent_blueprint(asset, native)
    migration = unreal.SampleBlueprintMigrationLibrary
    for event, function in EVENTS.items():
        if not migration.forward_isolated_event_override(asset, graph, event, function):
            raise RuntimeError(f"Cannot forward {event}. Discard unsaved changes.")
    if editor.get_blueprint_parent_class(asset) != native or not editor.compile_blueprint(asset):
        raise RuntimeError("The Look At zone failed to compile.")
    assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
    if not assets.save_loaded_asset(asset, only_if_is_dirty=False):
        raise RuntimeError(f"Could not save the Look At zone. Previous copy: {backup}")
    unreal.log("Native Look At zone installed.")


if __name__ == "__main__":
    migrate()
