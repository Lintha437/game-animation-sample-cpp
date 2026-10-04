"""Move level-block offset handling to C++ while preserving button bindings."""

from hashlib import sha256
from pathlib import Path
import shutil

import unreal


ASSET = "/Game/Levels/LevelPrototyping/LevelBlock"
REFERENCE = "2dda286a19ec6a9998fbba59f5ad1f4b712ee233fad341a770548b9e9a78e2e1"
EVENTS = {"Randomize Offset": "RandomizeBlockOffset", "Reset Offset": "ResetBlockOffset"}


def migrate():
    descriptor = Path(unreal.Paths.get_project_file_path()).resolve()
    if descriptor.name != "GameAnimSampleCpp.uproject":
        raise RuntimeError("Run the migration in the native project.")
    editor = unreal.BlueprintEditorLibrary
    asset = unreal.load_asset(ASSET)
    native = unreal.load_class(None, "/Script/GameAnimationSampleCpp.SampleLevelBlock")
    dirty = {item.get_name() for item in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()}
    if not asset or not native or ASSET in dirty:
        raise RuntimeError("Cannot load a clean level block and its native parent.")
    parent = editor.get_blueprint_parent_class(asset)
    if parent == native:
        unreal.log("Level block offsets already migrated.")
        return

    package = descriptor.parent / "Content" / (ASSET.removeprefix("/Game/") + ".uasset")
    graphs = {graph.get_name(): graph for graph in editor.list_graphs(asset)}
    if (parent != unreal.Actor.static_class() or not editor.can_reparent_blueprint(asset, native)
            or not {"EventGraph", "UpdateText"} <= graphs.keys()
            or sha256(package.read_bytes()).hexdigest() != REFERENCE):
        raise RuntimeError("The level block differs from the reference.")
    backup = descriptor.parent / "Saved/BlueprintMigration/SandboxBlock" / package.name
    if backup.exists() and sha256(backup.read_bytes()).hexdigest() != REFERENCE:
        raise RuntimeError(f"The existing block backup differs from the reference: {backup}")
    backup.parent.mkdir(parents=True, exist_ok=True)
    if not backup.exists():
        shutil.copy2(package, backup)

    editor.reparent_blueprint(asset, native)
    migration = unreal.SampleBlueprintMigrationLibrary
    for event, function in EVENTS.items():
        if not migration.forward_isolated_event_override(asset, graphs["EventGraph"], event, function):
            raise RuntimeError(f"Cannot forward {event}. Discard unsaved changes.")
    if not migration.replace_mover_input_function(asset, graphs["UpdateText"], "RefreshBlockText", "None"):
        raise RuntimeError("Cannot forward the block label update. Discard unsaved changes.")
    if not editor.compile_blueprint(asset):
        raise RuntimeError("The migrated level block failed to compile.")
    assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
    if not assets.save_loaded_asset(asset, only_if_is_dirty=False):
        raise RuntimeError(f"Could not save the level block. Previous copy: {backup}")
    unreal.log("Native level block offsets installed.")


if __name__ == "__main__":
    migrate()
