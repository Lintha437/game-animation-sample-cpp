"""Install the ragdoll transition gate after the parent Mover migration."""

from pathlib import Path
import runpy


if __name__ == "__main__":
    installer = runpy.run_path(str(Path(__file__).with_name("migrate_mover_mode_transitions.py")))
    installer["migrate"](installer["CHILD"])
