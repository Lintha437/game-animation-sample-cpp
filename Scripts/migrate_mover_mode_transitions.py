"""Install the audited Mover presentation hooks and guarded mode dispatch."""

from hashlib import sha256
import os
from pathlib import Path
from tempfile import NamedTemporaryFile
import unreal


BASE = "/Game/Blueprints/SandboxCharacter_Mover"
CHILD = "/Game/Blueprints/SandboxCharacter_Mover_Ragdoll"
SOURCE = 2
INSTALLED = 1


def _dirty(path):
    return any(package.get_name() == path
               for package in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages())


def _read_package(package):
    if package.is_symlink() or not package.is_file():
        raise RuntimeError(f"Missing package: {package}")
    for suffix in (".uexp", ".ubulk", ".uptnl", ".m.ubulk", ".upayload"):
        sidecar = package.with_suffix(suffix)
        if sidecar.exists() or sidecar.is_symlink():
            raise RuntimeError(f"Unsupported package sidecar: {sidecar}")
    payload = package.read_bytes()
    if not payload:
        raise RuntimeError(f"Empty source package: {package}")
    return payload


def _verify_backup(backup, digest):
    if (backup.is_symlink() or not backup.is_file()
            or sha256(backup.read_bytes()).hexdigest() != digest):
        raise RuntimeError(f"Missing or invalid migration backup: {backup}")


def _backup(descriptor, path):
    """Publish a complete content-addressed backup without replacing existing data."""
    if path not in (BASE, CHILD):
        raise RuntimeError(f"Unsupported Mover asset: {path}")
    package = descriptor.parent / "Content" / (path.removeprefix("/Game/") + ".uasset")
    payload = _read_package(package)
    digest = sha256(payload).hexdigest()
    backup = (descriptor.parent / "Saved" / "BlueprintMigration"
              / "MoverModeTransitions" / digest / package.name)
    backup.parent.mkdir(parents=True, exist_ok=True)
    if not backup.exists() and not backup.is_symlink():
        with NamedTemporaryFile(dir=backup.parent, prefix=backup.name + ".", delete=False) as output:
            temporary = Path(output.name)
            try:
                output.write(payload)
                output.flush()
                os.fsync(output.fileno())
                output.close()
                try:
                    os.link(temporary, backup)
                except FileExistsError:
                    pass
            finally:
                output.close()
                temporary.unlink()
    _verify_backup(backup, digest)
    if backup.read_bytes() != payload or _read_package(package) != payload:
        raise RuntimeError(f"Source or backup changed during preflight: {package}")
    return package, backup, digest


def migrate(path=BASE):
    if path not in (BASE, CHILD):
        raise RuntimeError(f"Unsupported Mover asset: {path}")
    descriptor = Path(unreal.Paths.get_project_file_path()).resolve()
    if descriptor.name != "GameAnimSampleCpp.uproject" or not descriptor.is_file():
        raise RuntimeError("Run in the native project.")
    migration = getattr(unreal, "SampleMoverBlueprintMigrationLibrary", None)
    if migration is None:
        raise RuntimeError("The standalone editor migration library must be available.")
    asset = unreal.load_asset(path)
    if not asset or _dirty(path):
        raise RuntimeError(f"Load the clean native project Blueprint: {path}")
    if path == CHILD:
        base = unreal.load_asset(BASE)
        if not base or _dirty(BASE) or not migration.verify_compiled_mover_mode_transitions(base):
            raise RuntimeError("Install and save the parent Mover transition dispatch first.")
    state = migration.classify_mover_mode_transitions(asset)
    if state == INSTALLED:
        if not migration.verify_compiled_mover_mode_transitions(asset):
            raise RuntimeError(f"Installed graph lacks a verified compiled implementation: {path}")
        unreal.log(f"Mover transition dispatch already verified: {path}")
        return
    if state != SOURCE:
        raise RuntimeError(f"Unsupported or partially migrated graph: {path}")
    package, backup, digest = _backup(descriptor, path)
    _verify_backup(backup, digest)
    if (_dirty(path) or sha256(_read_package(package)).hexdigest() != digest
            or migration.classify_mover_mode_transitions(asset) != SOURCE):
        raise RuntimeError(f"Blueprint changed after backup: {path}")
    if migration.install_mover_mode_transitions(asset) != INSTALLED:
        raise RuntimeError(f"Migration rejected. Discard unsaved changes. Backup: {backup}")
    if not migration.compile_and_verify_mover_mode_transitions(asset):
        raise RuntimeError(f"Compile or graph verification failed. Discard unsaved changes. Backup: {backup}")
    _verify_backup(backup, digest)
    if (not migration.verify_compiled_mover_mode_transitions(asset)
            or sha256(_read_package(package)).hexdigest() != digest or not _dirty(path)):
        raise RuntimeError(f"Pre-save state changed; package was not saved. Backup: {backup}")
    assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
    if not assets.save_loaded_asset(asset, only_if_is_dirty=True):
        raise RuntimeError(f"Save failed. Backup: {backup}")
    _verify_backup(backup, digest)
    if (_dirty(path) or not migration.verify_compiled_mover_mode_transitions(asset)
            or sha256(_read_package(package)).hexdigest() == digest):
        raise RuntimeError(f"Post-save verification failed. Backup: {backup}")
    unreal.log(f"Mover transition dispatch compiled, verified and saved: {path}")


if __name__ == "__main__":
    migrate()
