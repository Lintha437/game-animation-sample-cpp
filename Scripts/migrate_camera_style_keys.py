"""Replace the sandbox pawns' camera style key bindings with native input."""

from pathlib import Path
import shutil

import unreal


TOGGLE = "use_native_camera_style_keys"
PAWNS = {"SandboxCharacter_CMC": "SampleCmcCharacter", "SandboxCharacter_Mover": "SampleMoverCharacter"}


def migrate(pawns=PAWNS, toggle=TOGGLE, backup_group="CameraStyleKeys", remove="remove_sandbox_camera_keys"):
    descriptor = Path(unreal.Paths.get_project_file_path()).resolve()
    if descriptor.name != "GameAnimSampleCpp.uproject":
        raise RuntimeError("Run the migration in the native project.")
    editor = unreal.BlueprintEditorLibrary
    assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
    migration = unreal.SampleBlueprintMigrationLibrary
    dirty = {item.get_name() for item in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()}
    changed = False
    for name, native in pawns.items():
        path = f"/Game/Blueprints/{name}"
        asset = unreal.load_asset(path)
        parent = editor.get_blueprint_parent_class(asset) if asset else None
        graph = editor.find_event_graph(asset) if asset else None
        if not graph or not parent or parent.get_path_name() != f"/Script/GameAnimationSampleCpp.{native}" or path in dirty:
            raise RuntimeError(f"Load a clean migrated pawn: {path}")
        if unreal.get_default_object(editor.generated_class(asset)).get_editor_property(toggle):
            continue

        package = descriptor.parent / f"Content/Blueprints/{name}.uasset"
        backup = descriptor.parent / "Saved/BlueprintMigration" / backup_group / package.name
        if backup.exists() and backup.read_bytes() != package.read_bytes():
            raise RuntimeError(f"The input backup differs from the current asset: {backup}")
        backup.parent.mkdir(parents=True, exist_ok=True)
        if not backup.exists():
            shutil.copy2(package, backup)
        if not getattr(migration, remove)(asset, graph) or not editor.compile_blueprint(asset):
            raise RuntimeError(f"Cannot replace the shared {backup_group} input graph on {name}. Discard unsaved changes.")
        unreal.get_default_object(editor.generated_class(asset)).set_editor_property(toggle, True)
        if not assets.save_loaded_asset(asset, only_if_is_dirty=False):
            raise RuntimeError(f"Could not save {name}. Previous copy: {backup}")
        changed = True
    unreal.log(f"Native {backup_group} installed." if changed else f"{backup_group} already migrated.")


if __name__ == "__main__":
    migrate()
