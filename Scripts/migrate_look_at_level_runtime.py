"""Install the native forwarding calls in the Look At POI level script."""

from datetime import datetime, timezone
from hashlib import sha256
import json
from pathlib import Path
import shutil

import unreal


PROJECT_DESCRIPTOR_NAME = "GameAnimSampleCpp.uproject"
WORLD_ASSET_PATH = "/Game/Levels/LookAtPOI_Level.LookAtPOI_Level"
WORLD_PACKAGE_PATH = "/Game/Levels/LookAtPOI_Level"
LEVEL_SCRIPT_PATH = WORLD_ASSET_PATH + ":PersistentLevel.LookAtPOI_Level"
MAP_RELATIVE_PATH = Path("Content/Levels/LookAtPOI_Level.umap")
BACKUP_RELATIVE_DIRECTORY = Path("Saved/BlueprintMigration/LookAtLevelRuntime")


def file_sha256(path):
    return sha256(path.read_bytes()).hexdigest()


def dirty_package_paths():
    editor_utils = unreal.EditorLoadingAndSavingUtils
    packages = list(editor_utils.get_dirty_map_packages())
    packages.extend(editor_utils.get_dirty_content_packages())
    return sorted({package.get_path_name() for package in packages if package})


def require_clean_packages(operation):
    dirty = dirty_package_paths()
    if dirty:
        raise RuntimeError(
            f"Cannot {operation} while packages have unsaved changes: {', '.join(dirty)}"
        )


def project_paths():
    descriptor = Path(unreal.Paths.get_project_file_path()).resolve()
    if not descriptor.is_file() or descriptor.name != PROJECT_DESCRIPTOR_NAME:
        raise RuntimeError(
            f"Run this migration in the existing {PROJECT_DESCRIPTOR_NAME} project"
        )

    project_root = descriptor.parent
    map_file = (project_root / MAP_RELATIVE_PATH).resolve()
    if project_root not in map_file.parents or not map_file.is_file():
        raise RuntimeError(f"The target map is missing from the project: {map_file}")

    backup_directory = project_root / BACKUP_RELATIVE_DIRECTORY
    return descriptor, project_root, map_file, backup_directory


def open_target_level(map_file):
    require_clean_packages("open the Look At POI level")
    editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    world = editor.get_editor_world()
    if not world or world.get_path_name() != WORLD_ASSET_PATH:
        world = unreal.EditorLoadingAndSavingUtils.load_map(str(map_file))

    if not world or world.get_path_name() != WORLD_ASSET_PATH:
        raise RuntimeError("The Editor could not open /Game/Levels/LookAtPOI_Level")

    require_clean_packages("begin the Look At POI migration")
    return world


def validate_backup(backup, manifest, descriptor, original_sha, allow_create):
    if backup.exists() and not backup.is_file():
        raise RuntimeError(f"The migration backup path is not a file: {backup}")
    if manifest.exists() and not manifest.is_file():
        raise RuntimeError(f"The migration manifest path is not a file: {manifest}")

    if not backup.exists():
        if not allow_create or manifest.exists():
            raise RuntimeError(f"The original level backup is missing: {backup}")
        backup.parent.mkdir(parents=True, exist_ok=True)
        temporary_backup = backup.with_suffix(backup.suffix + ".tmp")
        shutil.copy2(descriptor.parent / MAP_RELATIVE_PATH, temporary_backup)
        temporary_backup.replace(backup)

    backup_sha = file_sha256(backup)
    if backup_sha != original_sha:
        raise RuntimeError(
            f"The existing level backup does not match the pre-save package: {backup}"
        )

    expected_manifest = {
        "schema_version": 1,
        "project_descriptor": PROJECT_DESCRIPTOR_NAME,
        "map_package": WORLD_PACKAGE_PATH,
        "map_relative_path": MAP_RELATIVE_PATH.as_posix(),
        "original_sha256": backup_sha,
    }
    if manifest.exists():
        current_manifest = json.loads(manifest.read_text(encoding="utf-8"))
        if current_manifest != expected_manifest:
            raise RuntimeError(f"The existing level backup manifest is incompatible: {manifest}")
    else:
        if not allow_create:
            raise RuntimeError(f"The original level backup manifest is missing: {manifest}")
        temporary_manifest = manifest.with_suffix(".json.tmp")
        temporary_manifest.write_text(
            json.dumps(expected_manifest, indent=2) + "\n", encoding="utf-8"
        )
        temporary_manifest.replace(manifest)

    return backup_sha


def write_report(report_path, descriptor, backup, result, before_sha, after_sha, backup_sha):
    report = {
        "timestamp_utc": datetime.now(timezone.utc).isoformat(),
        "project_descriptor": descriptor.name,
        "map_package": WORLD_PACKAGE_PATH,
        "map_relative_path": MAP_RELATIVE_PATH.as_posix(),
        "result": result,
        "before_sha256": before_sha,
        "after_sha256": after_sha,
        "backup_relative_path": backup.relative_to(descriptor.parent).as_posix(),
        "backup_sha256": backup_sha,
    }
    temporary_report = report_path.with_suffix(".json.tmp")
    temporary_report.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    temporary_report.replace(report_path)


def migrate():
    descriptor, project_root, map_file, backup_directory = project_paths()
    open_target_level(map_file)

    script_blueprint = unreal.find_object(
        None, LEVEL_SCRIPT_PATH, unreal.LevelScriptBlueprint
    )
    runtime_class = unreal.load_class(
        None, "/Script/GameAnimationSampleCpp.SampleLookAtLevelRuntime"
    )
    migration = getattr(unreal, "SampleLookAtLevelMigrationLibrary", None)
    if not script_blueprint or not runtime_class or not migration:
        raise RuntimeError(
            "The loaded level script and both compiled native migration classes are required"
        )

    classification = migration.classify_level_command_chains(script_blueprint)
    if classification == 0:
        raise RuntimeError(
            "The level script does not match the supported original or migrated graph"
        )
    if classification == 1:
        unreal.log("Look At POI level runtime forwarding is already installed. No map changes.")
        return
    if classification != 2:
        raise RuntimeError(f"Unexpected level graph classification: {classification}")

    backup = backup_directory / map_file.name
    manifest = backup_directory / "manifest.json"
    report_path = backup_directory / "install.json"
    before_sha = file_sha256(map_file)

    backup_sha = validate_backup(backup, manifest, descriptor, before_sha, True)
    mutation_result = migration.forward_level_command_chains(script_blueprint)
    if mutation_result != 2:
        raise RuntimeError(
            f"The level graph changed or migration failed after backup creation. Result: {mutation_result}. Backup: {backup}"
        )

    if not unreal.BlueprintEditorLibrary.compile_blueprint(script_blueprint):
        raise RuntimeError(f"The Look At POI level script failed to compile. Backup: {backup}")

    if migration.classify_level_command_chains(script_blueprint) != 1:
        raise RuntimeError(
            f"The compiled level script failed its five-node structural verification. Backup: {backup}"
        )

    dirty = set(dirty_package_paths())
    unexpected_dirty = dirty.difference({WORLD_PACKAGE_PATH})
    if unexpected_dirty:
        raise RuntimeError(
            "Compilation dirtied packages outside the target level. Nothing was saved: "
            + ", ".join(sorted(unexpected_dirty))
        )
    dirty_maps = {
        package.get_path_name()
        for package in unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
        if package
    }
    if WORLD_PACKAGE_PATH not in dirty_maps:
        raise RuntimeError("The migrated level package is not marked dirty. Nothing was saved.")

    level_editor = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    if not level_editor.save_current_level():
        raise RuntimeError(f"The Look At POI level could not be saved. Backup: {backup}")

    after_sha = file_sha256(map_file)
    if after_sha == before_sha:
        raise RuntimeError(f"The saved map did not change after migration. Backup: {backup}")
    if dirty_package_paths():
        raise RuntimeError("The Editor still reports dirty packages after saving the target level")
    if file_sha256(backup) != backup_sha:
        raise RuntimeError(f"The original level backup changed during installation: {backup}")

    write_report(
        report_path, descriptor, backup, "installed",
        before_sha, after_sha, backup_sha,
    )
    unreal.log("Look At POI level runtime forwarding compiled, structurally verified, and saved.")


if __name__ == "__main__":
    migrate()
