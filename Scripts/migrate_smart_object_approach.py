"""Move the Smart Object approach metrics and player cancellation state to C++."""

from pathlib import Path
import runpy


if __name__ == "__main__":
    installer = runpy.run_path(str(Path(__file__).with_name("migrate_mover_runtime.py")))
    installer["migrate"](
        "/Game/Blueprints/SmartObjects/AC_SmartObjectAnimation",
        "/Script/GameAnimationSampleCpp.SampleSmartObjectInteractionComponent",
        events={},
        backup_group="SmartObjectApproach",
        function_migration=("EventGraph", "forward_player_interaction_cleanup"),
        function_forwards=(("NPC Approach Angle and Distance", "GetNpcApproachMetrics"),),
    )
