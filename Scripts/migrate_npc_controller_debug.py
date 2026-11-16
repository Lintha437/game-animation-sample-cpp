"""Preserve AIC_NPC_SmartObject.Debug and install its audited native body."""

from hashlib import sha256
from pathlib import Path
import shutil

import unreal


PACKAGE = "/Game/Blueprints/AI/AIC_NPC_SmartObject"
ASSET = PACKAGE + ".AIC_NPC_SmartObject"
PACKAGE_FILE = Path("Content/Blueprints/AI/AIC_NPC_SmartObject.uasset")
BACKUP_DIRECTORY = Path("Saved/BlueprintMigration/NpcControllerDebug")


def digest(path):
    return sha256(path.read_bytes()).hexdigest()


def dirty_packages():
    saving = unreal.EditorLoadingAndSavingUtils
    packages = list(saving.get_dirty_content_packages()) + list(saving.get_dirty_map_packages())
    return {package.get_path_name() for package in packages if package}


def migrate():
    descriptor = Path(unreal.Paths.get_project_file_path()).resolve()
    if not descriptor.is_file() or descriptor.name != "GameAnimSampleCpp.uproject":
        raise RuntimeError("Run the NPC Debug migration in the native sample project.")
    source = descriptor.parent / PACKAGE_FILE
    if not source.is_file():
        raise RuntimeError(f"Missing NPC controller package: {source}")
    asset = unreal.load_asset(ASSET)
    native = unreal.load_class(None, "/Script/GameAnimationSampleCpp.SampleNpcController")
    migration = getattr(unreal, "SampleNpcControllerDebugMigration", None)
    if not asset or not native or not migration:
        raise RuntimeError("The NPC controller and both compiled native migration classes are required.")
    dirty = dirty_packages()
    if PACKAGE in dirty:
        raise RuntimeError("Save or revert the NPC controller's pending changes before migration.")

    before = digest(source)
    state = migration.classify_npc_debug(asset)
    if state == 1:
        if digest(source) != before or dirty_packages() != dirty:
            raise RuntimeError("The NPC Debug no-op unexpectedly modified a package.")
        unreal.log(f"NPC Debug native forward already installed. SHA-256: {before}")
        return
    if state != 2:
        raise RuntimeError("The NPC Debug graph does not match the supported original or native forward.")
    if digest(source) != before or dirty_packages() != dirty:
        raise RuntimeError("NPC Debug preflight unexpectedly modified a package.")

    backup = descriptor.parent / BACKUP_DIRECTORY / source.name
    if not backup.exists():
        backup.parent.mkdir(parents=True, exist_ok=True)
        temporary = backup.with_suffix(".uasset.tmp")
        shutil.copy2(source, temporary)
        temporary.replace(backup)
    if not backup.is_file() or digest(backup) != before:
        raise RuntimeError(f"The existing NPC controller backup differs from the original: {backup}")

    if migration.forward_npc_debug(asset) != 1:
        raise RuntimeError(f"NPC Debug in-memory verification failed. Discard unsaved changes. Backup: {backup}")
    if not unreal.BlueprintEditorLibrary.compile_blueprint(asset):
        raise RuntimeError(f"NPC controller failed to compile. Discard unsaved changes. Backup: {backup}")
    if migration.classify_npc_debug(asset) != 1:
        raise RuntimeError(f"The compiled NPC Debug forward failed verification. Backup: {backup}")
    after_compile = dirty_packages()
    if PACKAGE not in after_compile or after_compile - dirty - {PACKAGE}:
        raise RuntimeError(f"NPC Debug compilation did not isolate the controller package. Backup: {backup}")
    if digest(source) != before or digest(backup) != before:
        raise RuntimeError(f"The NPC controller or backup changed during migration. Backup: {backup}")

    assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
    if not assets.save_loaded_asset(asset, only_if_is_dirty=True):
        raise RuntimeError(f"Could not save the NPC controller. Backup: {backup}")
    after = digest(source)
    if (dirty_packages() != dirty or after == before or digest(backup) != before
            or migration.classify_npc_debug(asset) != 1):
        raise RuntimeError(f"The NPC controller save failed verification. Backup: {backup}")
    unreal.log(f"NPC Debug native forward compiled, verified and saved. Original: {before}, "
               f"installed: {after}, backup: {backup}")


if __name__ == "__main__":
    migrate()
