"""Forward the ragdoll sample target spawn and rotation to C++."""

from pathlib import Path
import runpy


if __name__ == "__main__":
    installer = runpy.run_path(str(Path(__file__).with_name("migrate_mover_runtime.py")))
    installer["migrate"](
        "/Game/Blueprints/SandboxCharacter_Mover_Ragdoll",
        "/Game/Blueprints/SandboxCharacter_Mover.SandboxCharacter_Mover_C",
        events={},
        backup_group="RagdollDemoSpawn",
        function_migration=("Demo_Spawn/RotateCharacter", "forward_ragdoll_demo_spawn"),
    )
