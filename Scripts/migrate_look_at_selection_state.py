"""Route Look At controller selection state to C++."""

from pathlib import Path
import runpy


if __name__ == "__main__":
    installer = runpy.run_path(str(Path(__file__).with_name("migrate_mover_runtime.py")))
    installer["migrate"](
        "/Game/IsolatedExamples/LookAtPOI/TestLevelContent/Controllers/PC_LookAtPOI",
        "/Script/GameAnimationSampleCpp.SampleLookAtController",
        events={},
        backup_group="LookAtSelectionState",
        function_migration=("EventGraph", "forward_look_at_zone_selection"),
    )
