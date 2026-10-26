"""Connect the ragdoll demo camera, reticle and hit response to C++."""

from pathlib import Path
import runpy


if __name__ == "__main__":
    installer = runpy.run_path(str(Path(__file__).with_name("migrate_mover_runtime.py")))
    installer["migrate"](
        "/Game/Blueprints/SandboxCharacter_Mover_Ragdoll",
        "/Game/Blueprints/SandboxCharacter_Mover.SandboxCharacter_Mover_C",
        events={},
        backup_group="RagdollDemoAim",
        function_forwards=(
            ("Demo_GetAim", "ResolveRagdollDemoAim"),
            ("Demo_SimpleCrosshair", "DrawRagdollDemoCrosshair"),
            ("Demo_SimpleShoot", "ApplyRagdollDemoHit"),
        ),
    )
