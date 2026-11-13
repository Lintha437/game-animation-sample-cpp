"""Forward the sandbox pawns' authored camera state queries to C++."""

import runpy
from pathlib import Path


if __name__ == "__main__":
    installer = runpy.run_path(str(Path(__file__).with_name("migrate_traversal_input_wrappers.py")))
    installer["migrate"]("Get_PropertiesForCamera", "forward_camera_properties", "CameraCharacterState",
                         "SampleCameraStateLibrary")
