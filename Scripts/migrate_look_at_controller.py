"""Move Look At POI possession and interface setup to the native controller."""

from hashlib import sha256
from pathlib import Path
import shutil

import unreal


ASSET = "/Game/IsolatedExamples/LookAtPOI/TestLevelContent/Controllers/PC_LookAtPOI"
REFERENCE = "cdd565521e7d580202f9027c298a93676c5fbace3b75357537775164a3547445"
EVENTS = {"Spawn Player": "EnterLookAtPlayerMode", "Spectator Mode": "EnterLookAtSpectatorMode"}


def migrate():
    descriptor = Path(unreal.Paths.get_project_file_path()).resolve()
    if descriptor.name != "GameAnimSampleCpp.uproject":
        raise RuntimeError("Run the migration in the native project.")
    editor = unreal.BlueprintEditorLibrary
    asset = unreal.load_asset(ASSET)
    native = unreal.load_class(None, "/Script/GameAnimationSampleCpp.SampleLookAtController")
    dirty = {item.get_name() for item in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()}
    if not asset or not native or ASSET in dirty:
        raise RuntimeError("Cannot load a clean Look At controller and its native parent.")
    parent = editor.get_blueprint_parent_class(asset)
    if parent == native:
        unreal.log("Look At controller already migrated.")
        return

    package = descriptor.parent / "Content" / (ASSET.removeprefix("/Game/") + ".uasset")
    graph = editor.find_event_graph(asset)
    if (parent != unreal.PlayerController.static_class() or not graph or not editor.can_reparent_blueprint(asset, native)
            or sha256(package.read_bytes()).hexdigest() != REFERENCE):
        raise RuntimeError("The Look At controller differs from the reference.")
    backup = descriptor.parent / "Saved/BlueprintMigration/LookAtController" / package.name
    if backup.exists() and sha256(backup.read_bytes()).hexdigest() != REFERENCE:
        raise RuntimeError(f"The Look At controller backup differs from the reference: {backup}")
    backup.parent.mkdir(parents=True, exist_ok=True)
    if not backup.exists():
        shutil.copy2(package, backup)

    editor.reparent_blueprint(asset, native)
    migration = unreal.SampleBlueprintMigrationLibrary
    for event, function in EVENTS.items():
        if not migration.forward_isolated_event_override(asset, graph, event, function):
            raise RuntimeError(f"Cannot forward {event}. Discard unsaved changes.")
    if not migration.remove_isolated_event_override(asset, graph, "ReceiveBeginPlay"):
        raise RuntimeError("Cannot remove the previous interface setup. Discard unsaved changes.")
    if editor.get_blueprint_parent_class(asset) != native or not editor.compile_blueprint(asset):
        raise RuntimeError("The Look At controller failed to compile.")
    assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
    if not assets.save_loaded_asset(asset, only_if_is_dirty=False):
        raise RuntimeError(f"Could not save the Look At controller. Previous copy: {backup}")
    unreal.log("Native Look At controller installed.")


if __name__ == "__main__":
    migrate()
