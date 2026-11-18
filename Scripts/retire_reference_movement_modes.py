"""Relocate validated legacy Mover mode and transition packages after the Editor exits."""

import hashlib
import json
from pathlib import Path


LEGACY_FILES = {
    "Content/Blueprints/MovementModes/BP_MovementMode_Falling.uasset",
    "Content/Blueprints/MovementModes/BP_MovementMode_Walking.uasset",
    "Content/Blueprints/MovementModes/BP_MovementMode_Slide.uasset",
    "Content/Blueprints/MovementModes/BP_MovementMode_Ragdoll.uasset",
    "Content/Blueprints/MovementModes/B_MovementMode_Flying.uasset",
    "Content/Blueprints/MovementModes/BP_MovementTransition_ToSlide.uasset",
    "Content/Blueprints/MovementModes/BP_MovementTransition_FromSlide.uasset",
}


def digest(path):
    checksum = hashlib.sha256()
    with path.open("rb") as source:
        for block in iter(lambda: source.read(1024 * 1024), b""):
            checksum.update(block)
    return checksum.hexdigest()


def retire_movement_mode_packages(descriptor):
    """Move exact, backed-up legacy Mover mode packages out of Content with the Editor closed."""
    descriptor = descriptor.resolve()
    if descriptor.name != "GameAnimSampleCpp.uproject" or not descriptor.is_file():
        raise RuntimeError("Select the native project descriptor.")
    project = descriptor.parent
    recovery = project / "Saved/BlueprintMigration/NativeMovementModes"
    plan = json.loads((recovery / "retirement-ready.json").read_text(encoding="utf-8"))
    if plan.get("version") != 1 or set(plan["source"]) != LEGACY_FILES or not plan["consumers"]:
        raise RuntimeError("Invalid Mover mode retirement record.")

    for relative, expected in plan["consumers"].items():
        path = (project / relative).resolve()
        if not path.is_relative_to(project / "Content") or digest(path) != expected:
            raise RuntimeError(f"Converted consumer changed after validation: {relative}")

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
