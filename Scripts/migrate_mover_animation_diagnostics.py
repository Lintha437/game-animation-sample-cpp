"""Migrate the audited CMC and Mover status line and diagnostic panel rows."""

from pathlib import Path

import unreal


ASSET_PATHS = (
    "/Game/Blueprints/SandboxCharacter_CMC_ABP",
    "/Game/Blueprints/SandboxCharacter_Mover_ABP",
)


def install():
    project = Path(unreal.Paths.get_project_file_path()).resolve()
    if project.name != "GameAnimSampleCpp.uproject":
        raise RuntimeError("Run this stage in the GameAnimSampleCpp project.")

    migration = getattr(unreal, "SampleMoverAnimationDiagnosticsMigration", None)
    states = getattr(unreal, "SampleAnimationStateLineMigrationState", None)
    if migration is None or states is None:
        raise RuntimeError("Build the runtime and editor modules before running this stage.")

    originals = []
    for asset_path in ASSET_PATHS:
        blueprint = unreal.load_asset(asset_path)
        if not blueprint:
            raise RuntimeError(f"Missing animation Blueprint {asset_path}.")
        state = migration.classify_state_line(blueprint)
        if state == states.INSTALLED:
            unreal.log(f"Animation diagnostic text formatters already installed in {asset_path}.")
            continue
        if state != states.ORIGINAL:
            raise RuntimeError(f"Strict animation diagnostic text audit rejected {asset_path}.")

        package = project.parent / ("Content" + asset_path.removeprefix("/Game") + ".uasset")
        backup = project.parent / "Saved/BlueprintMigration/AnimationStateLine" / package.name
        originals.append((asset_path, blueprint, package, backup))

    if not originals:
        return

    dirty_names = {item.get_name() for item in
                   unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()}
    backups = []
    for asset_path, blueprint, package, backup in originals:
        if asset_path in dirty_names:
            raise RuntimeError(f"Save or discard existing edits to {asset_path} before installation.")
        original_bytes = package.read_bytes()
        if backup.exists() and backup.read_bytes() != original_bytes:
            raise RuntimeError(f"Stale backup for {asset_path}: {backup}")
        backups.append((backup, original_bytes))

    for backup, original_bytes in backups:
        if not backup.exists():
            backup.parent.mkdir(parents=True, exist_ok=True)
            with backup.open("xb") as output:
                output.write(original_bytes)

    for asset_path, blueprint, _, backup in originals:
        if not migration.replace_state_line(blueprint):
            raise RuntimeError(f"Formatter replacement failed for {asset_path}. Backup: {backup}")
        if not migration.compile_and_verify_state_line(blueprint):
            raise RuntimeError(f"Compile or graph verification failed for {asset_path}. Backup: {backup}")

    assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
    for asset_path, blueprint, _, backup in originals:
        if migration.classify_state_line(blueprint) != states.INSTALLED:
            raise RuntimeError(f"Pre-save graph verification failed for {asset_path}. Backup: {backup}")
        if not assets.save_loaded_asset(blueprint, only_if_is_dirty=True):
            raise RuntimeError(f"Blueprint save failed for {asset_path}. Backup: {backup}")
        unreal.log(f"Animation diagnostic text formatters compiled, verified and saved in {asset_path}.")


if __name__ == "__main__":
    install()
