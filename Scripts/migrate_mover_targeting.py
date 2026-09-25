"""Keep the Mover target interface and route its device updates to C++."""

from pathlib import Path
import shutil

import unreal


ASSET = "/Game/Blueprints/SandboxCharacter_Mover"
FORWARDS = {
    "Update_TargetedActor": "UpdateMoverTarget",
    "Update_TwinStickMode": "UpdateMoverTwinStick",
    "Update_SmoothedAnalogInputAmount": "UpdateMoverAnalogDeflection",
}
EVENTS = {"AddTarget": "AddMoverTarget", "RemoveTarget": "RemoveMoverTarget"}


def migrate():
    descriptor = Path(unreal.Paths.get_project_file_path()).resolve()
    if descriptor.name != "GameAnimSampleCpp.uproject":
        raise RuntimeError("Run the migration in the native project.")
    editor = unreal.BlueprintEditorLibrary
    asset = unreal.load_asset(ASSET)
    parent = editor.get_blueprint_parent_class(asset) if asset else None
    if not parent or parent.get_path_name() != "/Script/GameAnimationSampleCpp.SampleMoverCharacter":
        raise RuntimeError("Apply the primary Mover migrations first.")
    if not unreal.get_default_object(editor.generated_class(asset)).get_editor_property("use_native_mover_device_input"):
        raise RuntimeError("Apply the Mover device input migration first.")

    def is_dirty():
        return any(package.get_name() == ASSET
                   for package in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages())

    if is_dirty():
        raise RuntimeError("The Mover pawn has unsaved changes.")
    graphs = {graph.get_name(): graph for graph in editor.list_graphs(asset)}
    if not (FORWARDS.keys() | {"EventGraph"}) <= graphs.keys():
        raise RuntimeError("The imported Mover target graphs differ from the reference.")

    migration = unreal.SampleBlueprintMigrationLibrary
    for name, native in FORWARDS.items():
        if not migration.replace_mover_input_function(asset, graphs[name], native, "None"):
            raise RuntimeError(f"Cannot forward {name}. Discard unsaved changes.")
    for event, native in EVENTS.items():
        if not migration.forward_isolated_event_override(asset, graphs["EventGraph"], event, native):
            raise RuntimeError(f"Cannot forward interface event {event}. Discard unsaved changes.")
    if not is_dirty():
        unreal.log("Mover targeting already migrated.")
        return

    package = descriptor.parent / "Content/Blueprints/SandboxCharacter_Mover.uasset"
    backup = descriptor.parent / "Saved/BlueprintMigration/MoverTargeting" / package.name
    backup.parent.mkdir(parents=True, exist_ok=True)
    if backup.exists() and backup.read_bytes() != package.read_bytes():
        raise RuntimeError(f"The Mover pawn differs from its backup: {backup}")
    if not backup.exists():
        shutil.copy2(package, backup)
    if not editor.compile_blueprint(asset):
        raise RuntimeError(f"Mover pawn compile failed. Previous copy: {backup}")
    assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
    if not assets.save_loaded_asset(asset, only_if_is_dirty=False):
        raise RuntimeError(f"Could not save the Mover pawn. Previous copy: {backup}")
    unreal.log("Native Mover targeting installed.")


if __name__ == "__main__":
    migrate()
