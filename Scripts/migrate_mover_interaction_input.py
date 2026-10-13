"""Bind the ragdoll pawn's interaction actions in C++ and keep authored animation selection."""

from pathlib import Path
import shutil

import unreal


ASSET = "/Game/Blueprints/SandboxCharacter_Mover_Ragdoll"
ACTIONS = {"/Game/Input/IA_Interact.IA_Interact", "/Game/Input/IA_Takedown.IA_Takedown"}
TOGGLE = "use_native_mover_interaction_input"


def migrate():
    descriptor = Path(unreal.Paths.get_project_file_path()).resolve()
    if descriptor.name != "GameAnimSampleCpp.uproject":
        raise RuntimeError("Run the migration in the native project.")
    editor = unreal.BlueprintEditorLibrary
    asset = unreal.load_asset(ASSET)
    parent = editor.get_blueprint_parent_class(asset) if asset else None
    graph = editor.find_event_graph(asset) if asset else None
    dirty = {item.get_name() for item in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()}
    if (not parent or parent.get_path_name() != "/Game/Blueprints/SandboxCharacter_Mover.SandboxCharacter_Mover_C"
            or not graph or ASSET in dirty):
        raise RuntimeError("Apply the primary Mover and ragdoll migrations first.")

    migration = unreal.SampleBlueprintMigrationLibrary
    previous_actions = set(migration.list_enhanced_input_actions(graph))
    enabled = unreal.get_default_object(editor.generated_class(asset)).get_editor_property(TOGGLE)
    if enabled and ACTIONS.isdisjoint(previous_actions):
        unreal.log("Mover interaction input already migrated.")
        return
    if enabled or not ACTIONS <= previous_actions:
        raise RuntimeError("The imported Mover interaction events differ from the reference.")
    package = descriptor.parent / "Content" / (ASSET.removeprefix("/Game/") + ".uasset")
    backup = descriptor.parent / "Saved/BlueprintMigration/MoverInteractionInput" / package.name
    if backup.exists() and backup.read_bytes() != package.read_bytes():
        raise RuntimeError(f"The Mover backup differs from the current asset: {backup}")
    backup.parent.mkdir(parents=True, exist_ok=True)
    if not backup.exists():
        shutil.copy2(package, backup)

    if (not migration.remove_enhanced_input_actions(asset, graph, sorted(ACTIONS))
            or set(migration.list_enhanced_input_actions(graph)) != previous_actions - ACTIONS
            or not migration.forward_isolated_event_override(asset, graph, "TryCharacterInteraction",
                                                             "RequestCharacterInteraction")):
        raise RuntimeError("Cannot isolate the original interaction dispatch. Discard unsaved changes.")
    if not editor.compile_blueprint(asset):
        raise RuntimeError("The ragdoll pawn failed to compile.")
    unreal.get_default_object(editor.generated_class(asset)).set_editor_property(TOGGLE, True)
    assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
    if not assets.save_loaded_asset(asset, only_if_is_dirty=False):
        raise RuntimeError(f"Could not save the ragdoll pawn. Previous copy: {backup}")
    unreal.log("Native Mover interaction input installed.")


if __name__ == "__main__":
    migrate()
