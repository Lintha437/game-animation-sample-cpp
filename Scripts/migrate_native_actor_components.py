"""Replace imported tick and visual-override ActorComponent Blueprints with native classes."""

import hashlib
import json
from pathlib import Path
import shutil

import unreal


LEGACY_COMPONENTS = (
    "/Game/Blueprints/AC_PreCMCTick",
    "/Game/Blueprints/AC_PostABPTick",
    "/Game/Blueprints/AC_VisualOverrideManager",
)
PAWN_BLUEPRINTS = (
    "/Game/Blueprints/SandboxCharacter_CMC",
    "/Game/Blueprints/SandboxCharacter_Mover",
    "/Game/Blueprints/SandboxCharacter_Mover_Ragdoll",
    "/Game/IsolatedExamples/LookAtPOI/TestLevelContent/Characters/Pawn_LookAt_POI_Char",
)
LEVELS = (
    "/Game/Levels/LookAtPOI_Level",
    "/Game/Levels/NPCLevel",
    "/Game/Levels/RagdollLevel",
)


def digest(path):
    checksum = hashlib.sha256()
    with path.open("rb") as source:
        for block in iter(lambda: source.read(1024 * 1024), b""):
            checksum.update(block)
    return checksum.hexdigest()


def check(condition, message):
    if not condition:
        raise RuntimeError(message)


def require_clean():
    saving = unreal.EditorLoadingAndSavingUtils
    if saving.get_dirty_content_packages() or saving.get_dirty_map_packages():
        raise RuntimeError("Save or discard outstanding changes before migrating ActorComponent assets.")


def relative_package_path(package):
    check(package.startswith("/Game/"), f"Unexpected non-game package: {package}")
    suffix = ".umap" if package.startswith("/Game/Levels/") else ".uasset"
    return Path("Content") / (package.removeprefix("/Game/") + suffix)


def package_file(project, package):
    path = project / relative_package_path(package)
    check(path.is_file() and not any(path.with_suffix(ext).exists()
          for ext in (".uexp", ".ubulk", ".uptnl", ".upayload")), f"Unsupported package layout: {path}")
    return path


def save_backup(project, recovery, package):
    path = package_file(project, package)
    backup = recovery / path.relative_to(project)
    backup.parent.mkdir(parents=True, exist_ok=True)
    if not backup.exists():
        with path.open("rb") as source, backup.open("xb") as destination:
            shutil.copyfileobj(source, destination)
    check(digest(backup) == digest(path), f"Package differs from its recovery copy: {package}")
    return digest(backup)


def migrate(prepare_retirement=True):
    descriptor = Path(unreal.Paths.get_project_file_path()).resolve()
    check(descriptor.name == "GameAnimSampleCpp.uproject", "Open the native sample project.")
    require_clean()
    project = descriptor.parent
    recovery = project / "Saved/BlueprintMigration/NativeActorComponents"
    plan_file = recovery / "conversion.json"
    assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
    editor = unreal.BlueprintEditorLibrary
    level_editor = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    library = getattr(unreal, "SampleActorComponentMigrationLibrary", None)
    check(library is not None, "Build the native Editor target before migrating ActorComponent assets.")

    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    registry.search_all_assets(True)
    options = unreal.AssetRegistryDependencyOptions(
        include_soft_package_references=True,
        include_hard_package_references=True,
        include_game_package_references=True,
        include_editor_only_package_references=True,
    )

    def referencers(package):
        return {str(item) for item in registry.get_referencers(package, options) or () if str(item) != package}

    if plan_file.exists():
        plan = json.loads(plan_file.read_text(encoding="utf-8"))
    else:
        for bp_path in PAWN_BLUEPRINTS:
            bp = unreal.load_asset(bp_path)
            check(bp and library.replace_actor_component_classes_in_blueprint(bp, True) >= 0,
                  f"ActorComponent validation failed for {bp_path}")
        originals = {
            package: save_backup(project, recovery, package)
            for package in (*PAWN_BLUEPRINTS, *LEVELS, *LEGACY_COMPONENTS)
        }
        plan = {"version": 1, "originals": originals}
        with plan_file.open("x", encoding="utf-8") as destination:
            json.dump(plan, destination, indent=2, sort_keys=True)

    check(plan.get("version") == 1, f"Unsupported ActorComponent conversion record: {plan_file}")
    for package, expected in plan["originals"].items():
        backup = recovery / relative_package_path(package)
        check(backup.is_file() and digest(backup) == expected, f"Invalid recovery copy: {package}")

    changed_files = []
    replaced_count = 0

    for bp_path in PAWN_BLUEPRINTS:
        bp = unreal.load_asset(bp_path)
        check(bp, f"Missing Blueprint: {bp_path}")
        count = library.replace_actor_component_classes_in_blueprint(bp, False)
        check(count >= 0, f"ActorComponent replacement failed for {bp_path}")
        bp_dirty = any(item.get_name() == bp_path for item in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages())
        if count > 0 or bp_dirty:
            check(editor.compile_blueprint(bp), f"Blueprint failed to compile after component replacement: {bp_path}")
            check(assets.save_loaded_asset(bp, only_if_is_dirty=True), f"Could not save Blueprint: {bp_path}")
            changed_files.append(str(package_file(project, bp_path)))
            replaced_count += count

    editor_sub = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    initial_world = editor_sub.get_editor_world()
    initial_map = initial_world.get_outer().get_path_name() if initial_world else None
    try:
        legacy_set = set(LEGACY_COMPONENTS)
        for level_path in LEVELS:
            require_clean()
            needs_level_save = any(level_path in referencers(pkg) for pkg in legacy_set)
            map_file = package_file(project, level_path)
            world = unreal.EditorLoadingAndSavingUtils.load_map(str(map_file))
            check(world is not None, f"Could not load level: {level_path}")
            refreshed = library.refresh_legacy_component_actors_in_level(world)
            check(refreshed >= 0, f"Could not clear legacy components in level: {level_path}")
            level_dirty = bool(unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages())
            if refreshed > 0 or level_dirty or needs_level_save:
                world.modify()
                check(level_editor.save_current_level(), f"Could not save level: {level_path}")
                changed_files.append(str(map_file))
                replaced_count += refreshed
    finally:
        if initial_map and initial_map.startswith("/Game/Levels/"):
            current = editor_sub.get_editor_world()
            if not current or current.get_outer().get_path_name() != initial_map:
                unreal.EditorLoadingAndSavingUtils.load_map(str(package_file(project, initial_map)))

    if changed_files:
        registry.scan_files_synchronous(changed_files, force_rescan=True)
    require_clean()

    if prepare_retirement:
        legacy_set = set(LEGACY_COMPONENTS)
        for package in LEGACY_COMPONENTS:
            remaining = referencers(package) - legacy_set
            check(not remaining, f"Remaining references prevent retirement: {package} ({sorted(remaining)})")
            if assets.does_asset_exist(package):
                check(digest(package_file(project, package)) == plan["originals"][package],
                      f"Legacy component asset changed since backup: {package}")
        ready = recovery / "retirement-ready.json"
        candidate = {
            "version": 1,
            "source": {
                relative_package_path(package).as_posix(): plan["originals"][package]
                for package in LEGACY_COMPONENTS
            },
            "consumers": {
                relative_package_path(package).as_posix(): digest(package_file(project, package))
                for package in (*PAWN_BLUEPRINTS, *LEVELS)
            },
        }
        if ready.exists():
            check(json.loads(ready.read_text(encoding="utf-8")) == candidate,
                  "ActorComponent retirement inputs changed after validation.")
        else:
            with ready.open("x", encoding="utf-8") as destination:
                json.dump(candidate, destination, indent=2, sort_keys=True)

    return {
        "consumers": len(PAWN_BLUEPRINTS) + len(LEVELS),
        "replaced": replaced_count,
        "saved": len(changed_files),
        "ready_for_retirement": prepare_retirement,
    }


if __name__ == "__main__":
    unreal.log(str(migrate()))
