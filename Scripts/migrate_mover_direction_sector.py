"""Prepare the Mover sector and rotation curve inputs in C++."""

from pathlib import Path
import runpy


if __name__ == "__main__":
    installer = runpy.run_path(str(Path(__file__).with_name("migrate_mover_runtime.py")))
    installer["migrate"](
        events={},
        backup_group="MoverDirectionSector",
        function_migration=("Get_MovementDirectionAndOffset", "forward_mover_direction_sector"),
    )
