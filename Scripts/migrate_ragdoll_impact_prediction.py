"""Forward ragdoll contact prediction to the native pawn."""

from pathlib import Path
import runpy


if __name__ == "__main__":
    installer = runpy.run_path(str(Path(__file__).with_name("migrate_mover_runtime.py")))
    installer["migrate"](
        "/Game/Blueprints/SandboxCharacter_Mover_Ragdoll",
        "/Game/Blueprints/SandboxCharacter_Mover.SandboxCharacter_Mover_C",
        events={},
        backup_group="RagdollImpactPrediction",
        function_forwards=(("Ragdoll_UpdateImpactDirection", "UpdatePredictedRagdollImpact"),),
    )
