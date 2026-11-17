"""Install the audited CMC jump and landing Foley volume policy."""

from hashlib import sha256
from pathlib import Path
import unreal


ASSET = "/Game/Blueprints/SandboxCharacter_CMC"
SOURCE = 2
INSTALLED = 1


def _dirty():
    return any(package.get_name() == ASSET
               for package in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages())


def _backup(descriptor):
    """Store an immutable, content-addressed copy before graph mutation."""
    package = descriptor.parent / "Content" / (ASSET.removeprefix("/Game/") + ".uasset")
    if not package.is_file():
        raise RuntimeError(f"Missing package: {package}")
    for suffix in (".uexp", ".ubulk", ".uptnl"):
        if package.with_suffix(suffix).exists():
            raise RuntimeError(f"Unsupported package sidecar: {package.with_suffix(suffix)}")
    payload = package.read_bytes()
    if not payload:
        raise RuntimeError(f"Empty source package: {package}")
    digest = sha256(payload).hexdigest()
    backup = (descriptor.parent / "Saved" / "BlueprintMigration"
              / "CmcFoleyVolume" / digest / package.name)
    backup.parent.mkdir(parents=True, exist_ok=True)
    try:
        with backup.open("xb") as output:
            output.write(payload)
    except FileExistsError:
        pass
    if backup.read_bytes() != payload or package.read_bytes() != payload:
        raise RuntimeError(f"Source or backup changed during preflight: {package}")
    return package, backup, digest


def migrate():
    descriptor = Path(unreal.Paths.get_project_file_path()).resolve()
    if descriptor.name != "GameAnimSampleCpp.uproject":
        raise RuntimeError("Run in the native GameAnimSampleCpp project.")
    migration = getattr(unreal, "SampleCmcFoleyBlueprintMigrationLibrary", None)
    if migration is None:
        raise RuntimeError("The CMC Foley editor migration library must be available.")
    asset = unreal.load_asset(ASSET)
    if not asset or _dirty():
        raise RuntimeError(f"Load the clean native project Blueprint: {ASSET}")
    state = migration.classify_cmc_foley_volume(asset)
    if state == INSTALLED:
        if not migration.verify_compiled_cmc_foley_volume(asset):
            raise RuntimeError(f"Installed graph lacks a verified compiled implementation: {ASSET}")
        unreal.log(f"CMC Foley volume already verified: {ASSET}")
        return
    if state != SOURCE:
        raise RuntimeError(f"Unsupported or partially migrated graph: {ASSET}")
    package, backup, digest = _backup(descriptor)
    if (_dirty() or sha256(package.read_bytes()).hexdigest() != digest
            or migration.classify_cmc_foley_volume(asset) != SOURCE):
        raise RuntimeError(f"Blueprint changed after backup: {ASSET}")
    if migration.install_cmc_foley_volume(asset) != INSTALLED:
        raise RuntimeError(f"Migration rejected. Discard unsaved changes. Backup: {backup}")
    if not migration.compile_and_verify_cmc_foley_volume(asset):
        raise RuntimeError(f"Compile or graph verification failed. Discard unsaved changes. Backup: {backup}")
    if (not migration.verify_compiled_cmc_foley_volume(asset)
            or sha256(package.read_bytes()).hexdigest() != digest or not _dirty()):
        raise RuntimeError(f"Pre-save state changed; package was not saved. Backup: {backup}")
    assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
    if not assets.save_loaded_asset(asset, only_if_is_dirty=True):
        raise RuntimeError(f"Save failed. Backup: {backup}")
    if (_dirty() or not migration.verify_compiled_cmc_foley_volume(asset)
            or sha256(package.read_bytes()).hexdigest() == digest):
        raise RuntimeError(f"Post-save verification failed. Backup: {backup}")
    unreal.log(f"CMC Foley volume compiled, verified and saved: {ASSET}")


if __name__ == "__main__":
    migrate()
