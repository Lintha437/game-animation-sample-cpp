"""Install native flight toggle inputs on the primary Mover pawn."""

from pathlib import Path
import runpy


if __name__ == "__main__":
    installer = runpy.run_path(str(Path(__file__).with_name("migrate_camera_style_keys.py")))
    installer["migrate"](
        {"SandboxCharacter_Mover": "SampleMoverCharacter"},
        "use_native_flight_toggle",
        "MoverFlightKeys",
        "remove_mover_flight_keys",
    )
