"""Enable the shared native multiplayer spawn placement in the Locomotor map."""

from hashlib import sha256
from pathlib import Path
import shutil

import unreal


ASSET = "/Game/IsolatedExamples/Locomotor/GM_Locomotor"
REFERENCE_SHA256 = "a5c92cb5a443fd40a8eae1ed884007e57c8f6bbbb991e7473de4e05eef585106"


def migrate():
    descriptor = Path(unreal.Paths.get_project_file_path()).resolve()
    if descriptor.name != "GameAnimSampleCpp.uproject":
        raise RuntimeError("Run this stage in GameAnimSampleCpp.")

    asset = unreal.load_asset(ASSET)
    native = unreal.load_class(None, "/Script/GameAnimationSampleCpp.SampleMultiplayerSpawnGameMode")
    if not asset or not native:
        raise RuntimeError("Load the Locomotor GameMode and native spawn GameMode before migration.")

    editor = unreal.BlueprintEditorLibrary
    parent = editor.get_blueprint_parent_class(asset)
    if parent == native:
        unreal.log("Locomotor multiplayer spawn GameMode already installed.")
        return

    package = descriptor.parent / "Content/IsolatedExamples/Locomotor/GM_Locomotor.uasset"
    if (parent != unreal.GameModeBase.static_class()
            or not editor.can_reparent_blueprint(asset, native)
            or sha256(package.read_bytes()).hexdigest() != REFERENCE_SHA256):
        raise RuntimeError("Locomotor GameMode differs from the expected reference asset.")
    if package.stem in {item.get_name() for item in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()}:
        raise RuntimeError("Save or discard pending Locomotor GameMode edits first.")

    backup = descriptor.parent / "Saved/BlueprintMigration/MultiplayerSpawn/GM_Locomotor.uasset"
    backup.parent.mkdir(parents=True, exist_ok=True)
    if backup.exists() and sha256(backup.read_bytes()).hexdigest() != REFERENCE_SHA256:
        raise RuntimeError("The existing Locomotor GameMode backup differs from the reference.")
    if not backup.exists():
        shutil.copy2(package, backup)

    editor.reparent_blueprint(asset, native)
    if editor.get_blueprint_parent_class(asset) != native or not editor.compile_blueprint(asset):
        raise RuntimeError("Locomotor GameMode failed to compile with its native parent.")
    assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
    if not assets.save_loaded_asset(asset, only_if_is_dirty=False):
        raise RuntimeError("Could not save the migrated Locomotor GameMode.")
    unreal.log("Locomotor multiplayer spawn GameMode installed.")


if __name__ == "__main__":
    migrate()
