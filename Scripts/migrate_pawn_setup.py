"""Forward sandbox possession setup and CMC proxy transitions to C++."""

from pathlib import Path
import shutil

import unreal


SETUP = {"SetupCamera": "ActivateSandboxCamera", "SetupInput": "InstallSandboxInputMapping"}
PAWNS = {
    "SandboxCharacter_CMC": ("SampleCmcCharacter", dict(SETUP, UpdatedMovementSimulated="UpdateSimulatedGroundTransition")),
    "SandboxCharacter_Mover": ("SampleMoverCharacter", SETUP),
}


def migrate():
    descriptor = Path(unreal.Paths.get_project_file_path()).resolve()
    if descriptor.name != "GameAnimSampleCpp.uproject":
        raise RuntimeError("Run the migration in the native project.")
    editor = unreal.BlueprintEditorLibrary
    assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
    migration = unreal.SampleBlueprintMigrationLibrary

    def dirty_packages():
        return {item.get_name() for item in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()}

    changed = False
    for name, (native, forwards) in PAWNS.items():
        path = f"/Game/Blueprints/{name}"
        asset = unreal.load_asset(path)
        parent = editor.get_blueprint_parent_class(asset) if asset else None
        graphs = {graph.get_name(): graph for graph in editor.list_graphs(asset)} if asset else {}
        if (not parent or parent.get_path_name() != f"/Script/GameAnimationSampleCpp.{native}"
                or path in dirty_packages() or not forwards.keys() <= graphs.keys()):
            raise RuntimeError(f"Load a clean, reparented pawn with its setup graphs: {path}")
        for graph, function in forwards.items():
            if not migration.replace_mover_input_function(asset, graphs[graph], function, "None"):
                raise RuntimeError(f"Cannot forward {graph} on {name}. Discard unsaved changes.")
        if path not in dirty_packages():
            continue

        package = descriptor.parent / f"Content/Blueprints/{name}.uasset"
        backup = descriptor.parent / "Saved/BlueprintMigration/PawnSetup" / package.name
        if backup.exists() and backup.read_bytes() != package.read_bytes():
            raise RuntimeError(f"The pawn backup differs from the current asset: {backup}")
        backup.parent.mkdir(parents=True, exist_ok=True)
        if not backup.exists():
            shutil.copy2(package, backup)
        if not editor.compile_blueprint(asset) or not assets.save_loaded_asset(asset, only_if_is_dirty=False):
            raise RuntimeError(f"Could not compile and save {name}. Previous copy: {backup}")
        changed = True
    unreal.log("Native sandbox pawn setup installed." if changed else "Sandbox pawn setup already migrated.")


if __name__ == "__main__":
    migrate()
