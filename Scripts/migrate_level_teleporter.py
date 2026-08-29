"""Move level portal dispatch and labeling to the native actor."""

import filecmp
from hashlib import sha256
from pathlib import Path
import shutil

import unreal


ASSET = "/Game/Levels/LevelPrototyping/Teleporter_Level"
NATIVE_PARENT = "/Script/GameAnimationSampleCpp.SampleLevelTeleporter"
REFERENCE_SHA256 = "9ba0c68063ace81ebc69db7b95b14b00ab1de2fd9c9e7b8f3ff09ee551eb3106"


def migrate():
    descriptor = Path(unreal.Paths.get_project_file_path()).resolve()
    if descriptor.name != "GameAnimSampleCpp.uproject":
        raise RuntimeError("Run the migration in the native sample project.")
    if any(package.get_name() == ASSET for package in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()):
        raise RuntimeError("The level teleporter has unsaved changes. Review them before migrating.")

    editor = unreal.BlueprintEditorLibrary
    blueprint = unreal.load_asset(ASSET)
    parent = unreal.load_class(None, NATIVE_PARENT)
    if not blueprint or not parent:
        raise RuntimeError("Import the sample and build the native Editor target.")

    graphs = {graph.get_name(): graph for graph in editor.list_graphs(blueprint)}
    previous_parent = editor.get_blueprint_parent_class(blueprint)
    if previous_parent == parent and not graphs:
        unreal.log("Level teleporter already migrated.")
        return

    package = descriptor.parent / "Content" / (ASSET.removeprefix("/Game/") + ".uasset")
    if (
        previous_parent != unreal.Actor.static_class()
        or set(graphs) != {"UserConstructionScript", "UpdateName", "EventGraph"}
        or sha256(package.read_bytes()).hexdigest() != REFERENCE_SHA256
        or not editor.can_reparent_blueprint(blueprint, parent)
    ):
        raise RuntimeError("Unexpected level teleporter graph or parent. Review the local asset.")

    original = unreal.get_default_object(editor.generated_class(blueprint))
    destination = str(original.get_editor_property("destination"))
    backup = descriptor.parent / "Saved" / "BlueprintMigration" / "LevelTeleporter" / package.name
    backup.parent.mkdir(parents=True, exist_ok=True)
    if backup.exists():
        if not filecmp.cmp(package, backup, shallow=False):
            raise RuntimeError("The local teleporter differs from its earlier backup.")
    else:
        shutil.copy2(package, backup)

    for graph in graphs.values():
        editor.remove_graph(blueprint, graph)
    editor.reparent_blueprint(blueprint, parent)
    if not editor.compile_blueprint(blueprint):
        raise RuntimeError("Level teleporter compilation failed.")
    updated = unreal.get_default_object(editor.generated_class(blueprint))
    if str(updated.get_editor_property("destination")) != destination:
        raise RuntimeError("The default travel destination changed.")
    editor_assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
    if not editor_assets.save_loaded_asset(blueprint, only_if_is_dirty=False):
        raise RuntimeError(f"Failed to save the level teleporter. Original copy: {backup}")
    unreal.log("Native level teleporter installed.")


if __name__ == "__main__":
    migrate()
