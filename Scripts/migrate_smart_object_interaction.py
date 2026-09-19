"""Install native collision and correction policies on the imported interaction component."""

from pathlib import Path
import shutil

import unreal


ASSET = "/Game/Blueprints/SmartObjects/AC_SmartObjectAnimation"
FORWARDS = {
    "Set Ignore Collision State": "UpdateInteractionCollision",
    "Set Ignore Character Movement Correction": "UpdateInteractionMovementCorrection",
}


def migrate():
    descriptor = Path(unreal.Paths.get_project_file_path()).resolve()
    if descriptor.name != "GameAnimSampleCpp.uproject":
        raise RuntimeError("Run the migration in the native project.")
    editor = unreal.BlueprintEditorLibrary
    asset = unreal.load_asset(ASSET)
    parent = unreal.load_class(None, "/Script/GameAnimationSampleCpp.SampleSmartObjectInteractionComponent")
    previous = editor.get_blueprint_parent_class(asset) if asset else None
    if not parent or previous not in (parent, unreal.ActorComponent.static_class()):
        raise RuntimeError("Build the native Editor and import the interaction component first.")

    def is_dirty():
        return any(package.get_name() == ASSET
                   for package in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages())

    if is_dirty():
        raise RuntimeError("The interaction component has unsaved changes.")
    graphs = {graph.get_name(): graph for graph in editor.list_graphs(asset)}
    if not FORWARDS.keys() <= graphs.keys():
        raise RuntimeError("The interaction component graphs differ from the reference.")
    package = descriptor.parent / f"Content/{ASSET.removeprefix('/Game/')}.uasset"
    backup = descriptor.parent / "Saved/BlueprintMigration/SmartObjectInteraction" / package.name
    if previous != parent:
        if not editor.can_reparent_blueprint(asset, parent):
            raise RuntimeError("Cannot reparent the imported interaction component.")
        backup.parent.mkdir(parents=True, exist_ok=True)
        if backup.exists() and backup.read_bytes() != package.read_bytes():
            raise RuntimeError(f"The interaction component differs from its backup: {backup}")
        if not backup.exists():
            shutil.copy2(package, backup)
        editor.reparent_blueprint(asset, parent)
    if editor.get_blueprint_parent_class(asset) != parent:
        raise RuntimeError("The interaction component's native parent was not installed.")

    for name, native in FORWARDS.items():
        if not unreal.SampleBlueprintMigrationLibrary.replace_mover_input_function(asset, graphs[name], native, "None"):
            raise RuntimeError(f"Cannot forward {name}. Discard unsaved changes.")
    if not is_dirty():
        unreal.log("Smart Object interaction collision already migrated.")
        return
    if not editor.compile_blueprint(asset):
        raise RuntimeError(f"Interaction component compile failed. Previous copy: {backup}")
    assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
    if not assets.save_loaded_asset(asset, only_if_is_dirty=False):
        raise RuntimeError(f"Could not save the interaction component. Previous copy: {backup}")
    unreal.log("Native Smart Object interaction collision installed.")


if __name__ == "__main__":
    migrate()
