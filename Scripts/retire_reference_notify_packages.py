"""Relocate validated legacy notify packages after the migration commandlet exits."""

import hashlib
import json
from pathlib import Path


LEGACY_FILES = {
    "Content/Blueprints/AnimNotifies/BP_AnimNotify_SimpleCameraShake.uasset",
    "Content/Blueprints/AnimNotifies/BP_NotifyState_OverrideMovementMode.uasset",
    "Content/Blueprints/AnimNotifies/BP_NotifyState_OverrideCapsuleCollision.uasset",
}


def digest(path):
    checksum = hashlib.sha256()
    with path.open("rb") as source:
        for block in iter(lambda: source.read(1024 * 1024), b""):
            checksum.update(block)
    return checksum.hexdigest()


def retire_notify_packages(descriptor):
    """Move exact, backed-up source packages out of Content with the project Editor closed."""
    descriptor = descriptor.resolve()
    if descriptor.name != "GameAnimSampleCpp.uproject" or not descriptor.is_file():
        raise RuntimeError("Select the native project descriptor.")
    project = descriptor.parent
    recovery = project / "Saved/BlueprintMigration/NativeAnimationNotifies"
    plan = json.loads((recovery / "retirement-ready.json").read_text(encoding="utf-8"))
    if plan.get("version") != 1 or set(plan["source"]) != LEGACY_FILES or not plan["animations"]:
        raise RuntimeError("Invalid notify retirement record.")

    for relative, expected in plan["animations"].items():
        path = (project / relative).resolve()
        if not path.is_relative_to(project / "Content") or digest(path) != expected:
            raise RuntimeError(f"Converted animation changed after validation: {relative}")

    moves = []
    for relative, expected in sorted(plan["source"].items()):
        source = (project / relative).resolve()
        backup = (recovery / relative).resolve()
        archive = (recovery / "Retired" / relative).resolve()
        if (not source.is_relative_to(project / "Content") or not backup.is_relative_to(recovery)
            or not archive.is_relative_to(recovery) or digest(backup) != expected):
            raise RuntimeError(f"Invalid retirement target or recovery copy: {relative}")
        if archive.is_file() and not source.exists() and digest(archive) == expected:
            continue
        if archive.exists() or digest(source) != expected:
            raise RuntimeError(f"Legacy package differs from its validated revision: {relative}")
        moves.append((source, archive))

    for source, archive in moves:
        archive.parent.mkdir(parents=True, exist_ok=True)
        source.rename(archive)
    return len(moves)
