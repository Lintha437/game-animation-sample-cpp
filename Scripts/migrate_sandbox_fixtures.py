"""Install native intra-level teleporters and the sandbox targeting dummy."""

from hashlib import sha256
from pathlib import Path
import shutil

import unreal


ROOT = "/Game/Levels/LevelPrototyping"
FIXTURES = {
    "TargetDummy": ("SampleTargetDummy", "1bb6f0159b2b6612ee9f30b473530656d268a9295dfdaa314428219f48922c67"),
    "Teleporter_Sender": (
        "SampleTeleporterSender", "b9c54e88cf265f9540d8428e4f394415f9a90a6bd4e8b61e0ef5f96355a1863b"
    ),
    "Teleporter_Destination": (
        "SampleTeleporterDestination", "c0843323cda8d08d32cfe1c8d3a6830e02b3495b93639e97dae64ed3732d7279"
    ),
}


def migrate():
    descriptor = Path(unreal.Paths.get_project_file_path()).resolve()
    if descriptor.name != "GameAnimSampleCpp.uproject":
        raise RuntimeError("Run the migration in the native project.")
    editor = unreal.BlueprintEditorLibrary
    assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
    dirty = {item.get_name() for item in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()}
    pending = []

    for name, (native_name, digest) in FIXTURES.items():
        path = f"{ROOT}/{name}"
        asset = unreal.load_asset(path)
        native = unreal.load_class(None, f"/Script/GameAnimationSampleCpp.{native_name}")
        if not asset or not native or path in dirty:
            raise RuntimeError(f"Cannot load a clean fixture and its native parent: {path}")
        parent = editor.get_blueprint_parent_class(asset)
        if parent == native:
            continue

        package = descriptor.parent / "Content" / (path.removeprefix("/Game/") + ".uasset")
        if (parent != unreal.Actor.static_class() or not editor.can_reparent_blueprint(asset, native)
                or sha256(package.read_bytes()).hexdigest() != digest):
            raise RuntimeError(f"The fixture differs from the reference: {path}")
        backup = descriptor.parent / "Saved/BlueprintMigration/SandboxFixtures" / package.name
        if backup.exists() and sha256(backup.read_bytes()).hexdigest() != digest:
            raise RuntimeError(f"The fixture backup differs from the reference: {backup}")
        pending.append((asset, native, package, backup))

    for asset, native, package, backup in pending:
        backup.parent.mkdir(parents=True, exist_ok=True)
        if not backup.exists():
            shutil.copy2(package, backup)
        for graph in editor.list_graphs(asset):
            editor.remove_graph(asset, graph)
        editor.reparent_blueprint(asset, native)
        if editor.get_blueprint_parent_class(asset) != native or not editor.compile_blueprint(asset):
            raise RuntimeError(f"The fixture failed to compile: {asset.get_path_name()}")

    if pending:
        button = unreal.load_asset(f"{ROOT}/LevelButton")
        if not button or not editor.compile_blueprint(button):
            raise RuntimeError("The level button could not resolve the inherited sender functions.")
    for asset, _, _, _ in pending:
        if not assets.save_loaded_asset(asset, only_if_is_dirty=False):
            raise RuntimeError(f"Could not save the fixture: {asset.get_path_name()}")
    unreal.log("Native sandbox fixtures installed." if pending else "Sandbox fixtures already migrated.")


if __name__ == "__main__":
    migrate()
