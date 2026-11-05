"""Publish accepted traversal results and send the original RPC from C++."""

from pathlib import Path
import runpy


if __name__ == "__main__":
    installer = runpy.run_path(str(Path(__file__).with_name("migrate_mover_runtime.py")))
    installer["migrate"](
        "/Game/Blueprints/AC_TraversalLogic",
        "/Script/GameAnimationSampleCpp.SampleTraversalComponent",
        events={},
        backup_group="TraversalDispatch",
        function_migration=("TryTraversalAction", "forward_traversal_action_dispatch"),
    )
