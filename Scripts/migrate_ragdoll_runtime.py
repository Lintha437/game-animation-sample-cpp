"""Forward the derived Mover pawn's ragdoll startup and tick to C++."""

from pathlib import Path
import runpy


if __name__ == "__main__":
    installer = runpy.run_path(str(Path(__file__).with_name("migrate_mover_runtime.py")))
    installer["migrate"](
        "/Game/Blueprints/SandboxCharacter_Mover_Ragdoll",
        "/Game/Blueprints/SandboxCharacter_Mover.SandboxCharacter_Mover_C",
        {"ReceiveBeginPlay": "InitializeRagdollRuntime", "ReceiveTick": "UpdateRagdollRuntime"},
        "RagdollRuntime",
    )
