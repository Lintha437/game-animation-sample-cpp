"""Route the primary and ragdoll Mover pawns' jump action through native code."""

from pathlib import Path
import shutil

import unreal


PRIMARY = "/Game/Blueprints/SandboxCharacter_Mover"
RAGDOLL = "/Game/Blueprints/SandboxCharacter_Mover_Ragdoll"
ACTION = "/Game/Input/IA_Jump.IA_Jump"
TOGGLE = "use_native_mover_jump_input"


def migrate():
    descriptor = Path(unreal.Paths.get_project_file_path()).resolve()
    if descriptor.name != "GameAnimSampleCpp.uproject":
        raise RuntimeError("Run the migration in the native project.")
    editor = unreal.BlueprintEditorLibrary
    primary = unreal.load_asset(PRIMARY)
    ragdoll = unreal.load_asset(RAGDOLL)
    parent = editor.get_blueprint_parent_class(primary) if primary else None
    if (not parent or parent.get_path_name() != "/Script/GameAnimationSampleCpp.SampleMoverCharacter"
            or not ragdoll or editor.get_blueprint_parent_class(ragdoll) != editor.generated_class(primary)):
        raise RuntimeError("Apply the primary and ragdoll Mover migrations first.")

    migration = unreal.SampleBlueprintMigrationLibrary
    assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
    dirty = {package.get_name() for package in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()}
    pending = []
    for path, pawn in ((PRIMARY, primary), (RAGDOLL, ragdoll)):
        event_graph = editor.find_event_graph(pawn)
        if path in dirty or not event_graph:
            raise RuntimeError(f"Pawn is dirty or missing its event graph: {path}")
        before = set(migration.list_enhanced_input_actions(event_graph))
        enabled = unreal.get_default_object(editor.generated_class(pawn)).get_editor_property(TOGGLE)
        if enabled and ACTION not in before:
            continue
        if enabled or ACTION not in before:
            raise RuntimeError(f"Pawn has a partial jump migration: {path}")
        package = descriptor.parent / f"Content/{path.removeprefix('/Game/')}.uasset"
        backup = descriptor.parent / "Saved/BlueprintMigration/MoverJump" / package.name
        if backup.exists() and backup.read_bytes() != package.read_bytes():
            raise RuntimeError(f"Pawn differs from its backup: {backup}")
        pending.append((pawn, event_graph, before, package, backup))

    if not pending:
        unreal.log("Mover jump already migrated.")
        return
    for pawn, event_graph, before, package, backup in pending:
        backup.parent.mkdir(parents=True, exist_ok=True)
        if not backup.exists():
            shutil.copy2(package, backup)
        if not migration.remove_enhanced_input_actions(pawn, event_graph, [ACTION]):
            raise RuntimeError(f"Cannot isolate the jump event: {pawn.get_path_name()}")
        if set(migration.list_enhanced_input_actions(event_graph)) != before - {ACTION}:
            raise RuntimeError(f"Unexpected input changes: {pawn.get_path_name()}")

    graph = next((g for g in editor.list_graphs(primary) if g.get_name() == "CanDoTraversal"), None)
    if not graph or not migration.replace_mover_input_function(primary, graph, "CanStartMoverTraversal", "ReturnValue"):
        raise RuntimeError("Could not forward the traversal gate. Discard unsaved changes.")
    for pawn, _, _, _, backup in pending:
        if not editor.compile_blueprint(pawn):
            raise RuntimeError(f"Pawn failed to compile. Previous copy: {backup}")
        unreal.get_default_object(editor.generated_class(pawn)).set_editor_property(TOGGLE, True)
    for pawn, _, _, _, backup in pending:
        if not assets.save_loaded_asset(pawn, only_if_is_dirty=False):
            raise RuntimeError(f"Could not save pawn. Previous copy: {backup}")
    unreal.log("Native Mover jump installed.")


if __name__ == "__main__":
    migrate()
