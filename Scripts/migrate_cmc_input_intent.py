"""Move the local CMC pawn's replicated input decisions to native handlers."""

from pathlib import Path
import shutil

import unreal


ASSET = "/Game/Blueprints/SandboxCharacter_CMC"
ACTION_NAMES = ("IA_Walk", "IA_Sprint", "IA_Strafe", "IA_Aim")
ACTION_PATHS = {f"/Game/Input/{name}.{name}" for name in ACTION_NAMES}
LEGACY_RPC = "UpdateInputState_Server"
NATIVE_RPC = "ServerCommitInputIntent"
# FUNC_NetFuncFlags from UObject/Script.h.
NETWORK_FLAGS = 0x012040C0


def migrate():
    descriptor = Path(unreal.Paths.get_project_file_path()).resolve()
    if descriptor.name != "GameAnimSampleCpp.uproject":
        raise RuntimeError("Run the migration in the native sample project.")
    if any(package.get_name() == ASSET for package in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()):
        raise RuntimeError("The CMC character has unsaved changes.")

    editor = unreal.BlueprintEditorLibrary
    migration = unreal.SampleBlueprintMigrationLibrary
    asset = unreal.load_asset(ASSET)
    if not asset:
        raise RuntimeError("Import the reference sample and apply the previous CMC migrations.")
    graphs = {graph.get_name(): graph for graph in editor.list_graphs(asset)}
    if "EventGraph" not in graphs or "CanPerformJumpAction" not in graphs:
        raise RuntimeError("Unexpected CMC event graph or montage gate.")
    defaults = unreal.get_default_object(editor.generated_class(asset))
    if not defaults.get_editor_property("use_native_input_handlers"):
        raise RuntimeError("Run the CMC movement, look, and crouch input migration first.")

    event_graph = graphs["EventGraph"]
    previous_actions = set(migration.list_enhanced_input_actions(event_graph))
    remaining = ACTION_PATHS.intersection(previous_actions)
    legacy_flags = migration.get_compiled_function_flags(asset, LEGACY_RPC)
    installed = defaults.get_editor_property("use_native_input_intent_handlers")
    if installed and not remaining and not legacy_flags:
        unreal.log("CMC input intent already migrated.")
        return
    if installed or remaining != ACTION_PATHS or not legacy_flags:
        raise RuntimeError("The CMC character contains a partial input intent migration.")
    native_flags = migration.get_compiled_function_flags(asset, NATIVE_RPC)
    if (legacy_flags & NETWORK_FLAGS) != (native_flags & NETWORK_FLAGS):
        raise RuntimeError("The native input intent RPC differs from the authored network settings.")

    package = descriptor.parent / "Content/Blueprints/SandboxCharacter_CMC.uasset"
    backup = descriptor.parent / "Saved/BlueprintMigration/CmcInputIntent" / package.name
    backup.parent.mkdir(parents=True, exist_ok=True)
    if backup.exists() and backup.read_bytes() != package.read_bytes():
        raise RuntimeError(f"The existing input intent backup differs from the current package: {backup}")
    if not backup.exists():
        shutil.copy2(package, backup)

    old_state = defaults.get_editor_property("character_input_state").export_text()
    if not migration.remove_enhanced_input_actions(asset, event_graph, sorted(ACTION_PATHS)):
        raise RuntimeError(f"The input intent chains could not be isolated. Previous copy: {backup}")
    if not migration.remove_custom_event(asset, event_graph, LEGACY_RPC):
        raise RuntimeError(f"The authored server event could not be isolated. Previous copy: {backup}")
    if not editor.compile_blueprint(asset):
        raise RuntimeError(f"The CMC character failed to compile. Previous copy: {backup}")
    if (
        set(migration.list_enhanced_input_actions(event_graph)) != previous_actions - ACTION_PATHS
        or migration.get_compiled_function_flags(asset, LEGACY_RPC)
    ):
        raise RuntimeError(f"Unexpected remaining bindings. Previous copy: {backup}")
    defaults = unreal.get_default_object(editor.generated_class(asset))
    if defaults.get_editor_property("character_input_state").export_text() != old_state:
        raise RuntimeError(f"Authored input defaults changed. Previous copy: {backup}")
    defaults.set_editor_property("use_native_input_intent_handlers", True)
    if not unreal.get_editor_subsystem(unreal.EditorAssetSubsystem).save_loaded_asset(asset, only_if_is_dirty=False):
        raise RuntimeError(f"Failed to save the CMC character. Previous copy: {backup}")
    unreal.log("Native CMC input intent installed.")


if __name__ == "__main__":
    migrate()
