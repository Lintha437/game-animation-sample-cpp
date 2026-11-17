"""Replace three imported notify classes with their concrete native implementations."""

import hashlib
import json
from pathlib import Path
import shutil

import unreal


BASE = "/Game/Blueprints/AnimNotifies/"
NOTIFIES = {
    "BP_AnimNotify_SimpleCameraShake": (
        "SampleCameraShakeAnimNotify", "notify", {"Shake": "ShakeClass"},
        {"shake": "shake_class"}),
    "BP_NotifyState_OverrideMovementMode": (
        "SampleOverrideMovementModeAnimNotifyState", "notify_state_class",
        {"OverrideMovementMode": "ModeOnBegin", "EndingMovementMode": "ModeOnEnd"},
        {"override_movement_mode": "mode_on_begin", "ending_movement_mode": "mode_on_end"}),
    "BP_NotifyState_OverrideCapsuleCollision": (
        "SampleOverrideCapsuleCollisionAnimNotifyState", "notify_state_class",
        {"New Response": "Response", "ResetOnEnd": "bRestoreCapsuleProfile"},
        {"new_response": "response", "reset_on_end": "restore_capsule_profile"}),
}
EVENT_FIELDS = (
    "notify_name", "trigger_weight_threshold", "montage_tick_type", "notify_trigger_chance",
    "notify_filter_type", "notify_filter_lod", "can_be_filtered_via_request",
    "trigger_on_dedicated_server", "trigger_on_follower",
)


def digest(path):
    checksum = hashlib.sha256()
    with path.open("rb") as source:
        for block in iter(lambda: source.read(1024 * 1024), b""):
            checksum.update(block)
    return checksum.hexdigest()


def require_clean():
    saving = unreal.EditorLoadingAndSavingUtils
    if saving.get_dirty_content_packages() or saving.get_dirty_map_packages():
        raise RuntimeError("Save or reload outstanding changes before replacing animation notify instances.")


def check(condition, message):
    if not condition:
        raise RuntimeError(message)


def package_file(project, package):
    check(package.startswith("/Game/"), f"Unexpected non-game reference: {package}")
    path = project / "Content" / (package.removeprefix("/Game/") + ".uasset")
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


def value(obj, field):
    prop = obj.get_editor_property(field)
    if prop is None or isinstance(prop, (int, float, str, bool)):
        return prop
    return prop.get_path_name() if isinstance(prop, unreal.Object) else str(prop)


def events(animation):
    return unreal.AnimationLibrary.get_animation_notify_events(animation)


def event_metadata(event):
    library = unreal.AnimationLibrary
    return [library.get_anim_notify_event_trigger_time(event), library.get_anim_notify_event_duration(event),
            *(value(event, field) for field in EVENT_FIELDS)]


def gather(project, recovery, referencers, legacy):
    editor = unreal.BlueprintEditorLibrary
    library = unreal.SampleAnimationNotifyMigrationLibrary
    packages = {}
    for name, (parent, slot, mapping, fields) in NOTIFIES.items():
        source = BASE + name
        blueprint = unreal.load_asset(source)
        check(blueprint and not editor.list_graphs(blueprint), f"Expected a graphless notify: {source}")
        generated = editor.generated_class(blueprint)
        check(editor.get_blueprint_parent_class(blueprint).get_path_name() == f"/Script/GameAnimationSampleCpp.{parent}",
              f"Unexpected notify parent: {source}")
        legacy[name] = generated
        for package in referencers(source):
            animation = unreal.load_asset(package)
            check(isinstance(animation, unreal.AnimSequenceBase), f"Unknown notify consumer: {package}")
            matches = []
            for index, event in enumerate(events(animation)):
                instance = event.get_editor_property(slot)
                if instance and instance.get_class() == generated:
                    matches.append({"index": index, "metadata": event_metadata(event),
                                    "settings": {new: value(instance, old) for old, new in fields.items()}})
            check(matches and library.replace_animation_notify_instances(animation, generated, mapping, True) == len(matches),
                  f"Unsupported notify instance data in {package}")
            packages.setdefault(package, {})[name] = matches
    originals = {package: save_backup(project, recovery, package) for package in
                 (*packages, *(BASE + name for name in NOTIFIES))}
    check(all(any(name in group for group in packages.values()) for name in NOTIFIES), "Missing authored notify usages.")
    return {"version": 1, "originals": originals, "packages": packages}


def verified(animation, group):
    records = events(animation)
    for name, matches in group.items():
        parent, slot, _, _ = NOTIFIES[name]
        for row in matches:
            if row["index"] >= len(records):
                return False
            event = records[row["index"]]
            instance = event.get_editor_property(slot)
            if (not instance or instance.get_class().get_path_name() != f"/Script/GameAnimationSampleCpp.{parent}"
                or event_metadata(event) != row["metadata"]
                or {field: value(instance, field) for field in row["settings"]} != row["settings"]):
                return False
    return True


def migrate(prepare_retirement=True):
    descriptor = Path(unreal.Paths.get_project_file_path()).resolve()
    check(descriptor.name == "GameAnimSampleCpp.uproject", "Open the native sample project.")
    require_clean()
    project = descriptor.parent
    assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    registry.search_all_assets(True)
    options = unreal.AssetRegistryDependencyOptions(include_soft_package_references=True,
        include_hard_package_references=True, include_game_package_references=True,
        include_editor_only_package_references=True)

    def referencers(package):
        return {str(item) for item in registry.get_referencers(package, options) or () if str(item) != package}

    recovery = project / "Saved/BlueprintMigration/NativeAnimationNotifies"
    plan_file = recovery / "conversion.json"
    legacy = {}
    if plan_file.exists():
        plan = json.loads(plan_file.read_text(encoding="utf-8"))
    else:
        plan = gather(project, recovery, referencers, legacy)
        with plan_file.open("x", encoding="utf-8") as destination:
            json.dump(plan, destination, indent=2, sort_keys=True)
    check(plan.get("version") == 1, f"Unsupported conversion record: {plan_file}")
    for package, expected in plan["originals"].items():
        backup = recovery / "Content" / (package.removeprefix("/Game/") + ".uasset")
        check(backup.is_file() and digest(backup) == expected, f"Invalid recovery copy: {package}")

    changed = []
    library = unreal.SampleAnimationNotifyMigrationLibrary
    editor = unreal.BlueprintEditorLibrary
    for package, group in plan["packages"].items():
        animation = unreal.load_asset(package)
        check(isinstance(animation, unreal.AnimSequenceBase), f"Missing animation: {package}")
        if verified(animation, group):
            continue
        path = package_file(project, package)
        check(digest(path) == plan["originals"][package], f"Unexpected animation revision: {package}")
        for name, matches in group.items():
            if name not in legacy:
                old = unreal.load_asset(BASE + name)
                check(old, f"Missing source notify: {name}")
                legacy[name] = editor.generated_class(old)
            mapping = NOTIFIES[name][2]
            check(library.replace_animation_notify_instances(animation, legacy[name], mapping, False) == len(matches),
                  f"Notify conversion failed: {package}")
        check(verified(animation, group), f"Converted notify metadata or settings differ: {package}")
        check(assets.save_loaded_asset(animation, only_if_is_dirty=True), f"Could not save: {package}")
        changed.append(str(path))

    if changed:
        registry.scan_files_synchronous(changed, force_rescan=True)
    require_clean()
    if prepare_retirement:
        for name in NOTIFIES:
            path = BASE + name
            remaining = referencers(path)
            check(not remaining, f"Remaining references prevent retirement: {path} ({sorted(remaining)})")
            if assets.does_asset_exist(path):
                check(digest(package_file(project, path)) == plan["originals"][path],
                      f"Notify Blueprint changed since backup: {path}")
        ready = recovery / "retirement-ready.json"
        candidate = {
            "version": 1,
            "source": {str((Path("Content/Blueprints/AnimNotifies") / (name + ".uasset")).as_posix()):
                       plan["originals"][BASE + name] for name in NOTIFIES},
            "animations": {package_file(project, package).relative_to(project).as_posix():
                           digest(package_file(project, package)) for package in plan["packages"]},
        }
        if ready.exists():
            check(json.loads(ready.read_text(encoding="utf-8")) == candidate, "Retirement inputs changed after validation.")
        else:
            with ready.open("x", encoding="utf-8") as destination:
                json.dump(candidate, destination, indent=2, sort_keys=True)
    return {"animations": len(plan["packages"]), "instances": sum(len(rows) for group in plan["packages"].values()
            for rows in group.values()), "saved": len(changed), "ready_for_retirement": prepare_retirement}


if __name__ == "__main__":
    unreal.log(str(migrate()))
