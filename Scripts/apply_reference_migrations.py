"""Apply the native asset migrations to a locally acquired Game Animation Sample."""

import argparse
import os
from pathlib import Path
import runpy
import subprocess
import tempfile


# An imported asset can be updated by several stages. Run its prerequisite stages first.
MIGRATIONS = (
    "animation_tick_components",
    "camera_shake_notify",
    "level_teleporter",
    "cmc_gait",
    "cmc_input",
    "cmc_input_intent",
    "sandbox_game_mode",
    "sandbox_controller",
    "traversal_movement",
    "traversal_ledges",
    "traversal_clearance",
    "traversal_collision",
    "traversal_modes",
    "traversal_replay",
    "mover_state",
    "mover_input_pipeline",
    "mover_orientation",
    "ragdoll_state",
    "ragdoll_lifecycle",
    "ragdoll_physics",
    "smart_object_slots",
    "smart_object_interaction",
    "player_smart_object",
    "mover_device_input",
    "mover_jump",
    "cmc_jump",
    "cmc_ragdoll",
    "mover_targeting",
    "mover_falling_and_transitions",
    "sandbox_obstacles",
    "sandbox_fixtures",
    "sandbox_block",
    "sandbox_visuals",
    "visual_override",
    "locomotor_input",
    "look_at_controller",
    "look_at_zones",
    "look_at_pointer",
    "look_at_gizmo",
    "mover_interaction_input",
    "mover_paired_collision",
    "ragdoll_behavior",
    "ragdoll_trigger",
    "pawn_setup",
    "mover_runtime",
    "ragdoll_runtime",
    "cmc_runtime",
    "camera_style_keys",
    "mover_flight_keys",
    "mover_transition_crouch",
)


def apply_batch(start, checkpoint):
    import unreal

    project = Path(unreal.Paths.get_project_file_path()).resolve()
    if project.name != "GameAnimSampleCpp.uproject":
        raise RuntimeError("Run the migration in the native project.")
    if not (project.parent / "Content/Levels/DefaultLevel.umap").is_file():
        raise RuntimeError("Import the locally acquired sample before applying migrations.")
    if not hasattr(unreal, "SampleBlueprintMigrationLibrary"):
        raise RuntimeError("Build the GameAnimationSampleCppEditor target before applying migrations.")

    scripts = Path(__file__).resolve().parent
    stages = [scripts / f"migrate_{name}.py" for name in MIGRATIONS]
    if len(stages) != len(set(stages)) or set(stages) != set(scripts.glob("migrate_*.py")):
        raise RuntimeError("The migration order must list every stage exactly once.")

    for index, stage in enumerate(stages[start:], start + 1):
        unreal.log(f"Reference migration {index}/{len(stages)}: {stage.stem}")
        try:
            runpy.run_path(str(stage), run_name="__main__")
        except Exception as error:
            raise RuntimeError(f"Reference migration stopped at {stage.name}. See the preceding Editor log.") from error
        if index < len(stages) and unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages():
            # Recompiling a parent can dirty dependent assets that the stage did not migrate.
            # Leave those assets on disk unchanged and continue from a new Editor process.
            break
    Path(checkpoint).write_text(str(index), encoding="utf-8")


def apply(project, engine_root):
    if project.name != "GameAnimSampleCpp.uproject" or not project.is_file():
        raise ValueError("Select the native GameAnimSampleCpp.uproject descriptor.")
    editor = engine_root / "Engine/Binaries/Win64/UnrealEditor-Cmd.exe"
    if not editor.is_file():
        raise ValueError("Select the Unreal Engine 5.9 installation with --engine-root.")
    log_root = project.parent / "Saved/Logs"
    log_root.mkdir(parents=True, exist_ok=True)
    logs = Path(tempfile.mkdtemp(prefix="ReferenceMigration-", dir=log_root))
    runner = Path(__file__).resolve()

    with tempfile.TemporaryDirectory(prefix="SampleMigration-") as temp:
        driver = Path(temp) / "run_batch.py"
        checkpoint = Path(temp) / "checkpoint.txt"
        completed = 0
        while completed < len(MIGRATIONS):
            log = logs / f"{completed + 1:02d}-{MIGRATIONS[completed]}.log"
            checkpoint.unlink(missing_ok=True)
            driver.write_text(
                "import runpy\n"
                f"runner = runpy.run_path({str(runner)!r})\n"
                f"runner['apply_batch']({completed}, {str(checkpoint)!r})\n",
                encoding="utf-8",
            )
            result = subprocess.run(
                [str(editor), str(project), "-run=PythonScript", f"-script={driver}", "-Unattended",
                 "-NullRHI", "-NoSound", f"-abslog={log}"],
                stdout=subprocess.DEVNULL, check=False,
            )
            if result.returncode != 0 or not checkpoint.is_file():
                raise RuntimeError(f"Migration stopped. See {log}")
            next_stage = int(checkpoint.read_text(encoding="utf-8"))
            if not completed < next_stage <= len(MIGRATIONS):
                raise RuntimeError(f"The Editor returned an invalid stage checkpoint. See {log}")
            completed = next_stage
            print(f"Completed {completed}/{len(MIGRATIONS)} stages.", flush=True)
    print(f"Reference asset migrations complete. Logs: {logs}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("project", type=Path, help="Path to GameAnimSampleCpp.uproject.")
    parser.add_argument("--engine-root", type=Path, default=os.environ.get("UE_ROOT"),
                        help="Unreal Engine 5.9 installation. Defaults to UE_ROOT.")
    args = parser.parse_args()
    if args.engine_root is None:
        parser.error("Set UE_ROOT or pass --engine-root.")
    try:
        apply(args.project.resolve(), args.engine_root.resolve())
    except (OSError, ValueError, RuntimeError) as error:
        parser.exit(1, f"{error}\n")


if __name__ == "__main__":
    main()
