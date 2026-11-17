"""Install the native camera director's selection and crouch-rig policy."""

from pathlib import Path
import shutil

import unreal


ASSET = "/Game/Blueprints/Cameras/CameraDirector_SandboxCharacter"


def migrate():
    project = Path(unreal.Paths.get_project_file_path()).resolve()
    if project.name != "GameAnimSampleCpp.uproject":
        raise RuntimeError("Run this migration in the native sample project.")
    editor = unreal.BlueprintEditorLibrary
    blueprint = unreal.load_asset(ASSET)
    native = unreal.load_class(None, "/Script/GameAnimationSampleCpp.SampleCameraStateLibrary")
    if not blueprint or not native:
        raise RuntimeError("Import the camera director and build the native Editor target.")
    if any(package.get_name() == ASSET for package in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()):
        raise RuntimeError("Reload the dirty camera director before migration.")

    def graph():
        return next((item for item in editor.list_graphs(blueprint) if item.get_name() == "EventGraph"), None)

    package = project.parent / "Content/Blueprints/Cameras/CameraDirector_SandboxCharacter.uasset"
    backup = project.parent / "Saved/BlueprintMigration/CameraDirectorSelection" / package.name
    current = graph()
    if not current:
        raise RuntimeError("Missing camera director EventGraph.")
    forward = unreal.SampleBlueprintMigrationLibrary.forward_camera_director_selection
    if forward(blueprint, current, native, True):
        unreal.log("Native camera director policy already installed.")
        return
    if backup.exists() and backup.read_bytes() != package.read_bytes():
        raise RuntimeError(f"Camera director backup differs from the current asset: {backup}")
    backup.parent.mkdir(parents=True, exist_ok=True)
    if not backup.exists():
        shutil.copy2(package, backup)
    if not forward(blueprint, current, native):
        raise RuntimeError(f"Unexpected camera director graph or native signature. Original copy: {backup}")
    if not editor.compile_blueprint(blueprint):
        raise RuntimeError(f"Camera director compilation failed. Original copy: {backup}")
    current = graph()
    if not current or not forward(blueprint, current, native, True):
        raise RuntimeError(f"Camera director graph verification failed. Original copy: {backup}")
    if not unreal.get_editor_subsystem(unreal.EditorAssetSubsystem).save_loaded_asset(blueprint, only_if_is_dirty=False):
        raise RuntimeError(f"Camera director save failed. Original copy: {backup}")
    unreal.log("Native camera director policy installed.")


if __name__ == "__main__":
    migrate()
