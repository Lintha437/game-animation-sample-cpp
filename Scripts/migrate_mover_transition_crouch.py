"""Move crouch state updates at finalized Mover transitions into C++."""

from pathlib import Path
import runpy


if __name__ == "__main__":
    installer = runpy.run_path(str(Path(__file__).with_name("migrate_mover_runtime.py")))
    installer["migrate"](
        events={},
        backup_group="MoverTransitionCrouch",
        function_migration=("On_MovementModeChanged_PostFinalize", "forward_mover_transition_crouch"),
    )
