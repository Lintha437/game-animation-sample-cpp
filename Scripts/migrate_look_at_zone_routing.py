"""Install and verify native zone routing in the existing Look At controller asset."""

import hashlib
from pathlib import Path
import shutil
from tempfile import NamedTemporaryFile

import unreal


ASSET = "/Game/IsolatedExamples/LookAtPOI/TestLevelContent/Controllers/PC_LookAtPOI"
MIGRATED, LEGACY, ENTRY_ONLY, TARGET_UPGRADE = 1, 2, 3, 4


def digest(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def dirty_packages():
    io = unreal.EditorLoadingAndSavingUtils
    return {
        package.get_path_name()
        for package in (*io.get_dirty_content_packages(), *io.get_dirty_map_packages())
    }


def backup_package(package, backup, before):
    backup.parent.mkdir(parents=True, exist_ok=True)
    try:
        stream = backup.open("xb")
    except FileExistsError:
        pass
    else:
        with stream, package.open("rb") as source:
            shutil.copyfileobj(source, stream)
    if digest(backup) != before or digest(package) != before:
        raise RuntimeError(f"The current controller and pre-migration backup must match: {backup}")


def rollback(package, backup, before, migration, blueprint, state):
    if not package.is_file() or digest(package) != before:
        if digest(backup) != before:
            raise RuntimeError(f"The pre-migration backup failed validation: {backup}")
        with NamedTemporaryFile(dir=package.parent, suffix=".restore", delete=False) as stream:
            restore = Path(stream.name)
        try:
            shutil.copy2(backup, restore)
            if digest(restore) != before:
                raise RuntimeError("The staged controller restore failed validation.")
            restore.replace(package)
        finally:
            restore.unlink(missing_ok=True)
    if not migration.reload_zone_routing_package(blueprint):
        raise RuntimeError(f"The original disk image is present, but the Editor could not reload it: {package}")
    original = unreal.load_asset(ASSET)
    if not original or migration.classify_zone_routing(original) != state or ASSET in dirty_packages():
        raise RuntimeError("The reloaded controller failed the original graph or clean-package check.")


def migrate():
    descriptor = Path(unreal.Paths.get_project_file_path()).resolve()
    if descriptor.name != "GameAnimSampleCpp.uproject":
        raise RuntimeError("Open the native Game Animation Sample project before migrating zone routing.")
    package = descriptor.parent / "Content" / (ASSET.removeprefix("/Game/") + ".uasset")
    if not package.is_file() or any(package.with_suffix(ext).exists() for ext in (".uexp", ".ubulk", ".uptnl")):
        raise RuntimeError("Zone routing requires the controller's single-file Editor package.")
    migration = unreal.SampleLookAtZoneRoutingMigration
    blueprint = unreal.load_asset(ASSET)
    initial_dirty = dirty_packages()
    if not blueprint or ASSET in initial_dirty:
        raise RuntimeError("The Look At controller must be loaded and clean.")
    state = migration.classify_zone_routing(blueprint)
    if state == MIGRATED:
        unreal.log("Look At controller zone routing already passes the migrated graph audit.")
        return
    if state not in (LEGACY, ENTRY_ONLY, TARGET_UPGRADE):
        raise RuntimeError("The Look At controller fails the pre-migration audit. Check the graph and stop any play session.")

    backup_dir = descriptor.parent / "Saved/BlueprintMigration/LookAtZoneRouting"
    if state == ENTRY_ONLY:
        backup_dir /= "TerminalUpgrade"
    elif state == TARGET_UPGRADE:
        backup_dir /= "TargetUpgrade"
    backup = backup_dir / package.name
    before = digest(package)
    backup_package(package, backup, before)
    try:
        if not migration.migrate_zone_routing(blueprint):
            raise RuntimeError("Could not install the audited controller route.")
        if not unreal.BlueprintEditorLibrary.compile_blueprint(blueprint):
            raise RuntimeError("The migrated Look At controller failed Blueprint compilation.")
        if migration.classify_zone_routing(blueprint) != MIGRATED:
            raise RuntimeError("The compiled Look At controller failed the migrated graph audit.")
        dirty = dirty_packages()
        if dirty - initial_dirty != {ASSET} or digest(package) != before or digest(backup) != before:
            raise RuntimeError("The controller is not the sole newly dirty package or the disk images changed.")

        assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
        if not assets.save_loaded_asset(blueprint, only_if_is_dirty=True):
            raise RuntimeError("Could not save the Look At controller.")
        if dirty_packages() != initial_dirty or digest(package) == before or digest(backup) != before:
            raise RuntimeError("The saved controller or original backup failed verification.")
    except Exception as failure:
        try:
            rollback(package, backup, before, migration, blueprint, state)
        except Exception as recovery:
            raise RuntimeError(f"Zone routing failed ({failure}). Recovery failed ({recovery}). Backup: {backup}") from recovery
        raise RuntimeError(f"Zone routing failed and the original controller was restored: {failure}") from failure

    unreal.log(f"Look At controller zone routing compiled, verified, and saved. Backup: {backup}")


if __name__ == "__main__":
    migrate()
