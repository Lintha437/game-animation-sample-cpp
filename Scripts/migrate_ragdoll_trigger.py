"""Install native ragdoll input and forward contact and profile checks."""

from pathlib import Path
import shutil

import unreal


ASSET = "/Game/Blueprints/SandboxCharacter_Mover_Ragdoll"
ACTION = "/Game/Input/IA_TriggerRagdoll.IA_TriggerRagdoll"
TOGGLE = "use_native_mover_ragdoll_input"


def migrate():
    descriptor = Path(unreal.Paths.get_project_file_path()).resolve()
    if descriptor.name != "GameAnimSampleCpp.uproject":
        raise RuntimeError("Run the migration in the native project.")
    editor = unreal.BlueprintEditorLibrary
    asset = unreal.load_asset(ASSET)
    graphs = {graph.get_name(): graph for graph in editor.list_graphs(asset)} if asset else {}
    dirty = {item.get_name() for item in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()}
    if not {"OnRagdollHit", "EventGraph"} <= graphs.keys() or ASSET in dirty:
        raise RuntimeError("Load a clean ragdoll pawn with the original input and contact graphs.")

    migration = unreal.SampleBlueprintMigrationLibrary
    actions = set(migration.list_enhanced_input_actions(graphs["EventGraph"]))
    enabled = unreal.get_default_object(editor.generated_class(asset)).get_editor_property(TOGGLE)
    if enabled and ACTION not in actions:
        unreal.log("Ragdoll trigger already migrated.")
        return
    if enabled or ACTION not in actions:
        raise RuntimeError("The imported trigger differs from the reference.")
    package = descriptor.parent / "Content" / (ASSET.removeprefix("/Game/") + ".uasset")
    backup = descriptor.parent / "Saved/BlueprintMigration/RagdollTrigger" / package.name
    if backup.exists() and backup.read_bytes() != package.read_bytes():
        raise RuntimeError(f"The ragdoll backup differs from the current asset: {backup}")
    backup.parent.mkdir(parents=True, exist_ok=True)
    if not backup.exists():
        shutil.copy2(package, backup)

    if (not migration.remove_enhanced_input_actions(asset, graphs["EventGraph"], [ACTION])
            or set(migration.list_enhanced_input_actions(graphs["EventGraph"])) != actions - {ACTION}
            or not migration.replace_mover_input_function(asset, graphs["OnRagdollHit"], "HandleRagdollContact", "None")
            or not migration.forward_isolated_event_override(asset, graphs["EventGraph"], "OnDDCvarChanged",
                                                             "HandleRagdollProfileVariable")):
        raise RuntimeError("Could not forward the ragdoll input or contact logic. Discard unsaved changes.")
    if not editor.compile_blueprint(asset):
        raise RuntimeError("The ragdoll pawn failed to compile.")
    unreal.get_default_object(editor.generated_class(asset)).set_editor_property(TOGGLE, True)
    assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
    if not assets.save_loaded_asset(asset, only_if_is_dirty=False):
        raise RuntimeError(f"Could not save the ragdoll pawn. Previous copy: {backup}")
    unreal.log("Native ragdoll trigger installed.")


if __name__ == "__main__":
    migrate()
