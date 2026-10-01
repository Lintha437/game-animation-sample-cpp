"""Install native startup and overlap logic on the imported sandbox obstacles."""

from hashlib import sha256
from pathlib import Path
import shutil

import unreal


ROOT = "/Game/Levels/LevelPrototyping/Obstacles"
OBSTACLES = {
    "BP_ZeroGZone": ("SampleZeroGravityZone", "073e5abb903a1248ed1d1a0dbdb6b2c986585a33597cdf960f7295de83e1e83c"),
    "BP_PhysicsObstacle": ("SamplePhysicsObstacle", "6112f20f036a5609107dd1ebc08a95ff6b98e4bbc346db40448d2a0cf1d0fce2"),
    "BP_SimpleObstacle": ("SampleTimedObstacle", "e86a4962b9ce0cabc0c6a6c9e945e459e98bb23325651205d2f7333d3f6b6506"),
}


def migrate():
    descriptor = Path(unreal.Paths.get_project_file_path()).resolve()
    if descriptor.name != "GameAnimSampleCpp.uproject":
        raise RuntimeError("Run the migration in the native project.")

    editor = unreal.BlueprintEditorLibrary
    assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
    dirty = {item.get_name() for item in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()}
    pending = []
    for name, (native_name, digest) in OBSTACLES.items():
        path = f"{ROOT}/{name}"
        asset = unreal.load_asset(path)
        native = unreal.load_class(None, f"/Script/GameAnimationSampleCpp.{native_name}")
        if not asset or not native or path in dirty:
            raise RuntimeError(f"Cannot load a clean obstacle and its native parent: {path}")
        parent = editor.get_blueprint_parent_class(asset)
        if parent == native:
            continue

        package = descriptor.parent / "Content" / (path.removeprefix("/Game/") + ".uasset")
        graph = editor.find_event_graph(asset)
        if (parent != unreal.Actor.static_class() or not graph or not editor.can_reparent_blueprint(asset, native)
                or sha256(package.read_bytes()).hexdigest() != digest):
            raise RuntimeError(f"The obstacle differs from the reference: {path}")

        backup = descriptor.parent / "Saved/BlueprintMigration/SandboxObstacles" / package.name
        if backup.exists() and sha256(backup.read_bytes()).hexdigest() != digest:
            raise RuntimeError(f"The obstacle backup differs from the reference: {backup}")
        pending.append((asset, native, graph, package, backup, name == "BP_ZeroGZone"))

    for asset, native, graph, package, backup, remove_graph in pending:
        backup.parent.mkdir(parents=True, exist_ok=True)
        if not backup.exists():
            shutil.copy2(package, backup)
        if remove_graph:
            editor.remove_graph(asset, graph)
        elif not unreal.SampleBlueprintMigrationLibrary.remove_isolated_event_override(asset, graph, "ReceiveBeginPlay"):
            raise RuntimeError(f"Cannot isolate obstacle initialization: {asset.get_path_name()}")
        editor.reparent_blueprint(asset, native)
        if editor.get_blueprint_parent_class(asset) != native or not editor.compile_blueprint(asset):
            raise RuntimeError(f"The obstacle failed to compile: {asset.get_path_name()}")
        if not assets.save_loaded_asset(asset, only_if_is_dirty=False):
            raise RuntimeError(f"Could not save the obstacle: {asset.get_path_name()}")
    unreal.log("Native sandbox obstacle setup installed." if pending else "Sandbox obstacles already migrated.")


if __name__ == "__main__":
    migrate()
