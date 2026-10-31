"""Move traversal block spline initialization into the native base."""

from pathlib import Path
import runpy


if __name__ == "__main__":
    installer = runpy.run_path(str(Path(__file__).with_name("migrate_mover_runtime.py")))
    installer["migrate"](
        "/Game/Levels/LevelPrototyping/LevelBlock_Traversable",
        "/Game/Levels/LevelPrototyping/LevelBlock.LevelBlock_C",
        events={},
        backup_group="TraversalLedgeSetup",
        function_migration=("UserConstructionScript", "forward_traversal_ledge_setup"),
    )
