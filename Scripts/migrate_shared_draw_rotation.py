"""Install the native draw-rotation preamble in the two diagnostic AnimBP graphs."""

import os
from pathlib import Path
from tempfile import NamedTemporaryFile

import unreal


PROJECT_NAME = "GameAnimSampleCpp.uproject"
TARGETS = (
    ("/Game/Blueprints/SandboxCharacter_CMC_ABP", "Debug_ExperimentalStateMachine"),
    ("/Game/Blueprints/SandboxCharacter_Mover_ABP", "DebugDraws"),
)


def _find_graph(blueprint, graph_name):
    for graph in unreal.BlueprintEditorLibrary.list_graphs(blueprint):
        if graph.get_name() == graph_name:
            return graph
    return None


def _dirty_package_names():
    return {
        package.get_name()
        for package in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()
    }


def _backup_originals(originals, backup_root):
    backups = []
    for asset_path, package_path in originals:
        if not package_path.is_file():
            raise RuntimeError(f"Missing source package {package_path}")
        original_bytes = package_path.read_bytes()
        backup = backup_root / package_path.name
        if backup.exists() and backup.read_bytes() != original_bytes:
            raise RuntimeError(f"Backup differs from the original package for {asset_path}: {backup}")
        backups.append((package_path, backup, original_bytes))

    backup_root.mkdir(parents=True, exist_ok=True)
    for package_path, backup, original_bytes in backups:
        if not backup.exists():
            with NamedTemporaryFile(dir=backup_root, prefix=backup.name + ".", delete=False) as stream:
                temporary_backup = Path(stream.name)
                try:
                    stream.write(original_bytes)
                    stream.close()
                    # Publish a complete backup without replacing an existing one.
                    try:
                        os.link(temporary_backup, backup)
                    except FileExistsError:
                        pass
                finally:
                    stream.close()
                    temporary_backup.unlink()
        if backup.read_bytes() != original_bytes or package_path.read_bytes() != original_bytes:
            raise RuntimeError(f"Package or backup changed during backup creation: {package_path}")


def migrate():
    project_file = Path(unreal.Paths.get_project_file_path()).resolve()
    if project_file.name != PROJECT_NAME:
        raise RuntimeError(f"Expected {PROJECT_NAME}, got {project_file.name}")

    migration = getattr(unreal, "SampleAnimationDrawContextMigration", None)
    if migration is None or not unreal.load_class(
        None, "/Script/GameAnimationSampleCpp.SampleAnimationDrawContext"
    ):
        raise RuntimeError("Build and load both native sample modules before running this migration")

    graph_state = unreal.SampleDrawRotationGraphState
    targets = []
    originals = []
    for asset_path, graph_name in TARGETS:
        blueprint = unreal.load_asset(asset_path)
        graph = _find_graph(blueprint, graph_name) if blueprint else None
        state = migration.classify_shared_draw_rotation(blueprint, graph) if graph else graph_state.REJECTED
        targets.append((asset_path, graph_name, blueprint, graph, state))
        if state == graph_state.ORIGINAL:
            package_path = project_file.parent / "Content" / (asset_path.removeprefix("/Game/") + ".uasset")
            originals.append((asset_path, package_path))

    for asset_path, graph_name, _, _, state in targets:
        if state not in (graph_state.ORIGINAL, graph_state.INSTALLED):
            raise RuntimeError(f"Unsupported diagnostic graph {asset_path}:{graph_name}")

    dirty_targets = _dirty_package_names().intersection(asset_path for asset_path, _ in TARGETS)
    if dirty_targets:
        raise RuntimeError("Reload dirty target packages before migration: " + ", ".join(sorted(dirty_targets)))
    if not originals:
        unreal.log("Shared draw rotation is already installed in both diagnostic graphs.")
        return

    backup_root = project_file.parent / "Saved" / "BlueprintMigration" / "SharedDrawRotation"
    _backup_originals(originals, backup_root)

    _, _, cmc_blueprint, cmc_graph, _ = targets[0]
    _, _, mover_blueprint, mover_graph, _ = targets[1]
    if not migration.install_shared_draw_rotation_pair(
        cmc_blueprint, cmc_graph, mover_blueprint, mover_graph
    ):
        raise RuntimeError(f"Graph audit or connection failed. Original packages: {backup_root}")

    editor = unreal.BlueprintEditorLibrary
    changed_targets = [target for target in targets if target[4] == graph_state.ORIGINAL]
    for asset_path, _, blueprint, _, _ in changed_targets:
        if not editor.compile_blueprint(blueprint):
            raise RuntimeError(f"Compile failed for {asset_path}. Original packages: {backup_root}")

    for asset_path, graph_name, blueprint, _, _ in targets:
        graph = _find_graph(blueprint, graph_name)
        if not graph or migration.classify_shared_draw_rotation(blueprint, graph) != graph_state.INSTALLED:
            raise RuntimeError(
                f"Post-compile graph audit failed for {asset_path}:{graph_name}. "
                f"Original packages: {backup_root}"
            )

    assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
    for asset_path, _, blueprint, _, _ in changed_targets:
        if not assets.save_loaded_asset(blueprint, only_if_is_dirty=False):
            raise RuntimeError(f"Save failed for {asset_path}. Original packages: {backup_root}")

    unreal.log("Shared draw rotation compiled, structurally verified, and saved.")


if __name__ == "__main__":
    migrate()
