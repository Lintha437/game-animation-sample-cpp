"""Forward CMC runtime initialization and the pre-movement tick to native code."""

from pathlib import Path
import runpy


if __name__ == "__main__":
    installer = runpy.run_path(str(Path(__file__).with_name("migrate_mover_runtime.py")))
    installer["migrate"](
        "/Game/Blueprints/SandboxCharacter_CMC",
        "/Script/GameAnimationSampleCpp.SampleCmcCharacter",
        {"ReceiveBeginPlay": "InitializeCmcRuntime", "PreCMCTick": "UpdateBeforeCmcTick"},
        "CmcRuntime",
    )
