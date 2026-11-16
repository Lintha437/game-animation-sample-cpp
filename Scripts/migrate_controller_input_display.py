"""Install the native input-display forwarding in WB_SimpleController."""

from hashlib import sha256
from pathlib import Path
import shutil

import unreal


PACKAGE = "/Game/Widgets/WidgetData/WB_SimpleController"
ASSET = PACKAGE + ".WB_SimpleController"
PACKAGE_FILE = Path("Content/Widgets/WidgetData/WB_SimpleController.uasset")
BACKUP_DIRECTORY = Path("Saved/BlueprintMigration/ControllerInputDisplay")


def digest(path):
    return sha256(path.read_bytes()).hexdigest()


def dirty_packages():
    saving = unreal.EditorLoadingAndSavingUtils
    packages = list(saving.get_dirty_content_packages()) + list(saving.get_dirty_map_packages())
    return {package.get_path_name() for package in packages if package}


def migrate():
    descriptor = Path(unreal.Paths.get_project_file_path()).resolve()
    if not descriptor.is_file() or descriptor.name != "GameAnimSampleCpp.uproject":
        raise RuntimeError("Run the input-display migration in the native sample project.")

    source = descriptor.parent / PACKAGE_FILE
    if not source.is_file():
        raise RuntimeError(f"Missing controller widget package: {source}")
    asset = unreal.load_asset(ASSET)
    native = unreal.load_class(None, "/Script/GameAnimationSampleCpp.SampleControllerInputDisplay")
    migration = getattr(unreal, "SampleControllerInputDisplayMigration", None)
    if not asset or not native or not migration:
        raise RuntimeError("The controller widget and both compiled native migration classes are required.")
    dirty = dirty_packages()
    if PACKAGE in dirty:
        raise RuntimeError("Save or revert the controller widget's pending changes before migration.")

    state = migration.classify_input_display(asset)
    if state == 1:
        unreal.log("Controller input-display forwarding is already installed.")
        return
    if state != 2:
        raise RuntimeError("The controller widget does not match the supported original or forwarded graphs.")

    before = digest(source)
    backup = descriptor.parent / BACKUP_DIRECTORY / source.name
    if not backup.exists():
        backup.parent.mkdir(parents=True, exist_ok=True)
        temporary = backup.with_suffix(".uasset.tmp")
        shutil.copy2(source, temporary)
        temporary.replace(backup)
    if not backup.is_file() or digest(backup) != before:
        raise RuntimeError(f"The existing controller widget backup differs from the original: {backup}")

    if migration.forward_input_display(asset) != 1:
        raise RuntimeError(f"The in-memory rewrite failed structural verification. Backup: {backup}")
    if not unreal.BlueprintEditorLibrary.compile_blueprint(asset):
        raise RuntimeError(f"The controller widget failed to compile. Backup: {backup}")
    if migration.classify_input_display(asset) != 1:
        raise RuntimeError(f"The compiled widget failed structural verification. Backup: {backup}")

    after_compile = dirty_packages()
    if PACKAGE not in after_compile or after_compile - dirty - {PACKAGE}:
        raise RuntimeError(f"Compilation did not isolate the controller widget package. Backup: {backup}")
    if digest(source) != before or digest(backup) != before:
        raise RuntimeError(f"The original package or backup changed during migration. Backup: {backup}")

    assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
    if not assets.save_loaded_asset(asset, only_if_is_dirty=True):
        raise RuntimeError(f"Could not save the controller widget. Backup: {backup}")
    if PACKAGE in dirty_packages() or digest(source) == before:
        raise RuntimeError(f"The widget save could not be verified on disk. Backup: {backup}")
    unreal.log(f"Controller widget forwarding compiled, verified and saved. Original backup: {backup}")


if __name__ == "__main__":
    migrate()
