"""Replace the Mover debug draw and direction threshold graphs with native forwards."""

from pathlib import Path
import runpy


if __name__ == "__main__":
    installer = runpy.run_path(str(Path(__file__).with_name("migrate_mover_runtime.py")))
    installer["migrate"](
        events={},
        backup_group="MoverDebugDraw",
        function_migration=("Get_MovementDirectionThresholds", "forward_mover_direction_thresholds"),
        function_forwards=(("DebugDraws", "DrawMoverMovementDebug"),),
    )
