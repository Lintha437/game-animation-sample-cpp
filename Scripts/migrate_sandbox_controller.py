"""Install native sandbox controller input and remove the superseded GameMode cycle graphs."""

from hashlib import sha256
from pathlib import Path
import shutil

import unreal


CONTROLLER = "/Game/Blueprints/PC_Sandbox"
GAME_MODE = "/Game/Blueprints/GM_Sandbox"
CONTROLLER_PARENT = "/Script/GameAnimationSampleCpp.SampleSandboxPlayerController"
MODE_PARENT = "/Script/GameAnimationSampleCpp.SampleSandboxGameMode"
CONTROLLER_HASH = "a2d269e010a92e551651ad3ca4e0389dfc5a8fdc09c31edf5a71f201f13ebf30"
MODE_GRAPHS = {"CyclePawn", "CycleVisualOverride"}
NETWORK_FLAGS = 0x012040C0


def backup_package(root, asset):
    package = root / f"Content/Blueprints/{asset.get_name()}.uasset"
    backup = root / "Saved/BlueprintMigration/SandboxController" / package.name
    backup.parent.mkdir(parents=True, exist_ok=True)
    if backup.exists() and backup.read_bytes() != package.read_bytes():
        raise RuntimeError(f"Existing backup differs from the current asset: {backup}")
    if not backup.exists():
        shutil.copy2(package, backup)
    return backup


def save(asset):
    subsystem = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
    if not subsystem.save_loaded_asset(asset, only_if_is_dirty=False):
        raise RuntimeError(f"Failed to save {asset.get_path_name()}.")


def migrate():
    descriptor = Path(unreal.Paths.get_project_file_path()).resolve()
    if descriptor.name != "GameAnimSampleCpp.uproject":
        raise RuntimeError("Run the migration in the native sample project.")
    dirty = unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()
    if any(package.get_name() in (CONTROLLER, GAME_MODE) for package in dirty):
        raise RuntimeError("The sandbox controller or GameMode has unsaved changes.")

    editor = unreal.BlueprintEditorLibrary
    library = unreal.SampleBlueprintMigrationLibrary
    controller = unreal.load_asset(CONTROLLER)
    mode = unreal.load_asset(GAME_MODE)
    parent = unreal.load_class(None, CONTROLLER_PARENT)
    native_mode = unreal.load_class(None, MODE_PARENT)
    if not controller or not mode or not parent or editor.get_blueprint_parent_class(mode) != native_mode:
        raise RuntimeError("Build the Editor target and run the sandbox GameMode migration first.")

    graphs = {graph.get_name(): graph for graph in editor.list_graphs(controller)}
    mode_graphs = {graph.get_name(): graph for graph in editor.list_graphs(mode)}
    remaining_mode = MODE_GRAPHS.intersection(mode_graphs)
    if remaining_mode and remaining_mode != MODE_GRAPHS:
        raise RuntimeError("The GameMode contains a partial cycle migration.")

    old_parent = editor.get_blueprint_parent_class(controller)
    controller_migrated = old_parent == parent and "TeleportToTarget" not in graphs and "EventGraph" not in graphs
    if controller_migrated and not remaining_mode:
        unreal.log("Sandbox controller already migrated.")
        return

    if not controller_migrated:
        package = descriptor.parent / "Content/Blueprints/PC_Sandbox.uasset"
        if (
            old_parent != unreal.PlayerController.static_class()
            or not {"TeleportToTarget", "EventGraph"}.issubset(graphs)
            or sha256(package.read_bytes()).hexdigest() != CONTROLLER_HASH
        ):
            raise RuntimeError("The sandbox controller package differs from the supported reference.")
        backup = backup_package(descriptor.parent, controller)
        flags = {name: library.get_compiled_function_flags(controller, name) & NETWORK_FLAGS
                 for name in ("NextPawn", "NextVisualOverride")}
        distance = unreal.get_default_object(editor.generated_class(controller)).get_editor_property("TeleportMaxDistance")

        # The verified controller event graph contains only the three actions, their RPCs, and the mobile tick.
        editor.remove_graph(controller, graphs["EventGraph"])
        editor.remove_graph(controller, graphs["TeleportToTarget"])
        if not library.remove_declared_variable(controller, "TeleportMaxDistance"):
            raise RuntimeError(f"The original teleport distance field is missing. Previous copy: {backup}")
        editor.reparent_blueprint(controller, parent)
        if not editor.compile_blueprint(controller):
            raise RuntimeError(f"The sandbox controller failed to compile. Previous copy: {backup}")
        for name, expected in flags.items():
            if library.get_compiled_function_flags(controller, name) & NETWORK_FLAGS != expected:
                raise RuntimeError(f"Unexpected network flags for {name}. Previous copy: {backup}")
        defaults = unreal.get_default_object(editor.generated_class(controller))
        defaults.set_editor_property("target_teleport_distance", distance)
        save(controller)

    if remaining_mode:
        backup = backup_package(descriptor.parent, mode)
        for name in sorted(remaining_mode):
            editor.remove_graph(mode, mode_graphs[name])
        if not editor.compile_blueprint(mode):
            raise RuntimeError(f"The sandbox GameMode failed to compile. Previous copy: {backup}")
        save(mode)
    unreal.log("Native sandbox controller installed.")


if __name__ == "__main__":
    migrate()
