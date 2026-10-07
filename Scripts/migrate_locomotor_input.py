"""Install native input routing on the isolated Locomotor example."""

from hashlib import sha256
from pathlib import Path
import shutil

import unreal


ROOT = "/Game/IsolatedExamples/Locomotor"
INPUT_ASSETS = {
    "PC_Locomotor": (
        unreal.PlayerController, "SampleLocomotorController",
        "a7bc389bc92716046b1cc28f75781ff355268ce043fdb60069e76be9a68476f0"
    ),
    "BP_Walker": (
        unreal.Character, "SampleLocomotorWalker",
        "39d73a731d5f6033b90f06c87d9d37388f0ab61956401a7c0a81ecbba8d8fe90"
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

    for name, (old_class, native_name, digest) in INPUT_ASSETS.items():
        path = f"{ROOT}/{name}"
        asset = unreal.load_asset(path)
        native = unreal.load_class(None, f"/Script/GameAnimationSampleCpp.{native_name}")
        if not asset or not native or path in dirty:
            raise RuntimeError(f"Cannot load a clean Locomotor asset and its native parent: {path}")
        parent = editor.get_blueprint_parent_class(asset)
        if parent == native:
            continue

        package = descriptor.parent / "Content" / (path.removeprefix("/Game/") + ".uasset")
        event_graph = editor.find_event_graph(asset)
        if (parent != old_class.static_class() or not event_graph or not editor.can_reparent_blueprint(asset, native)
                or sha256(package.read_bytes()).hexdigest() != digest):
            raise RuntimeError(f"The Locomotor asset differs from the reference: {path}")
        backup = descriptor.parent / "Saved/BlueprintMigration/LocomotorInput" / package.name
        if backup.exists() and sha256(backup.read_bytes()).hexdigest() != digest:
            raise RuntimeError(f"The Locomotor backup differs from the reference: {backup}")
        pending.append((asset, native, event_graph, package, backup))

    for asset, native, event_graph, package, backup in pending:
        backup.parent.mkdir(parents=True, exist_ok=True)
        if not backup.exists():
            shutil.copy2(package, backup)
        editor.remove_graph(asset, event_graph)
        editor.reparent_blueprint(asset, native)
        if editor.get_blueprint_parent_class(asset) != native or not editor.compile_blueprint(asset):
            raise RuntimeError(f"The Locomotor asset failed to compile: {asset.get_path_name()}")
        if not assets.save_loaded_asset(asset, only_if_is_dirty=False):
            raise RuntimeError(f"Could not save the Locomotor asset: {asset.get_path_name()}")
    unreal.log("Native Locomotor input installed." if pending else "Locomotor input already migrated.")


if __name__ == "__main__":
    migrate()
