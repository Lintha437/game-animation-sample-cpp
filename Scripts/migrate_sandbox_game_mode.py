"""Install the native GameMode in the local sandbox Blueprint."""

from hashlib import sha256
from pathlib import Path
import shutil

import unreal


ASSET = "/Game/Blueprints/GM_Sandbox"
PARENT = "/Script/GameAnimationSampleCpp.SampleSandboxGameMode"
REFERENCE_HASH = "3e44e060c4aa136d00ae3326ea586a28d24d1439836d25e7f668e44c695632e6"
MIGRATED_GRAPHS = {"GetDefaultPawnClassForController", "ResetAllPlayers"}


def authored_defaults(obj):
    default_pawn = obj.get_editor_property("default_pawn_class")
    choices = obj.get_editor_property("PawnClasses_Soft")
    return (
        default_pawn.get_path_name() if default_pawn else None,
        tuple(choice.get_path_name() if choice else None for choice in choices),
    )


def migrate():
    descriptor = Path(unreal.Paths.get_project_file_path()).resolve()
    if descriptor.name != "GameAnimSampleCpp.uproject":
        raise RuntimeError("Run the migration in the native sample project.")
    if any(package.get_name() == ASSET for package in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()):
        raise RuntimeError("The sandbox GameMode has unsaved changes.")

    editor = unreal.BlueprintEditorLibrary
    asset = unreal.load_asset(ASSET)
    parent = unreal.load_class(None, PARENT)
    if not asset or not parent:
        raise RuntimeError("Import the reference sample and build the native Editor target.")
    old_parent = editor.get_blueprint_parent_class(asset)
    graphs = {graph.get_name(): graph for graph in editor.list_graphs(asset)}
    if old_parent == parent and not MIGRATED_GRAPHS.intersection(graphs):
        unreal.log("Sandbox GameMode already migrated.")
        return

    package = descriptor.parent / "Content/Blueprints/GM_Sandbox.uasset"
    if (
        old_parent != unreal.GameModeBase.static_class()
        or not MIGRATED_GRAPHS.issubset(graphs)
        or "EventGraph" not in graphs
        or sha256(package.read_bytes()).hexdigest() != REFERENCE_HASH
    ):
        raise RuntimeError("The sandbox package differs from the supported reference.")

    backup = descriptor.parent / "Saved/BlueprintMigration/SandboxGameMode" / package.name
    backup.parent.mkdir(parents=True, exist_ok=True)
    if backup.exists() and backup.read_bytes() != package.read_bytes():
        raise RuntimeError(f"An incompatible sandbox GameMode backup exists: {backup}")
    if not backup.exists():
        shutil.copy2(package, backup)

    original = authored_defaults(unreal.get_default_object(editor.generated_class(asset)))
    # The verified reference event graph contains only the console-variable binding and its callback.
    editor.remove_graph(asset, graphs["EventGraph"])
    for name in sorted(MIGRATED_GRAPHS):
        editor.remove_graph(asset, graphs[name])
    editor.reparent_blueprint(asset, parent)
    if not editor.compile_blueprint(asset):
        raise RuntimeError(f"The sandbox GameMode failed to compile. Previous copy: {backup}")
    if authored_defaults(unreal.get_default_object(editor.generated_class(asset))) != original:
        raise RuntimeError(f"Authored pawn choices changed. Previous copy: {backup}")
    if not unreal.get_editor_subsystem(unreal.EditorAssetSubsystem).save_loaded_asset(asset, only_if_is_dirty=False):
        raise RuntimeError(f"Failed to save the sandbox GameMode. Previous copy: {backup}")
    unreal.log("Native sandbox GameMode installed.")


if __name__ == "__main__":
    migrate()
