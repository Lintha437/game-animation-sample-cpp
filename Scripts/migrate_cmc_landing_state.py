"""Move CMC landing state and its retriggerable window to C++."""

from pathlib import Path
import runpy


if __name__ == "__main__":
    installer = runpy.run_path(str(Path(__file__).with_name("migrate_mover_runtime.py")))
    installer["migrate"](
        "/Game/Blueprints/SandboxCharacter_CMC",
        "/Script/GameAnimationSampleCpp.SampleCmcCharacter",
        events={},
        backup_group="CmcLandingState",
        function_migration=("EventGraph", "forward_cmc_landing_state"),
    )
