"""Forward the sandbox pawns' authored traversal query interfaces to C++."""

from pathlib import Path
import shutil

import unreal


PAWNS = {
    "SandboxCharacter_CMC": "SampleCmcCharacter",
    "SandboxCharacter_Mover": "SampleMoverCharacter",
}
NATIVE = "/Script/GameAnimationSampleCpp"


def migrate(graph_name="GetTraversalCheckInputs", forward="forward_traversal_check_inputs",
            backup_group="TraversalInputWrappers"):
    project = Path(unreal.Paths.get_project_file_path()).resolve()
    if project.name != "GameAnimSampleCpp.uproject":
        raise RuntimeError("Run this migration in the native sample project.")
    editor = unreal.BlueprintEditorLibrary
    library = unreal.load_class(None, f"{NATIVE}.SampleTraversalComponent")
    if not library:
        raise RuntimeError("Build the native traversal component before migration.")

    def dirty_packages():
        return {item.get_name() for item in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()}

    pending = []
    for name, native_parent in PAWNS.items():
        asset_path = f"/Game/Blueprints/{name}"
        asset = unreal.load_asset(asset_path)
        parent = editor.get_blueprint_parent_class(asset) if asset else None
        if not parent or parent.get_path_name() != f"{NATIVE}.{native_parent}" or asset_path in dirty_packages():
            raise RuntimeError(f"Load a clean migrated pawn: {asset_path}")
        graph = next((item for item in editor.list_graphs(asset)
                      if item.get_name() == graph_name), None)
        migration = unreal.SampleBlueprintMigrationLibrary
        if not graph or not getattr(migration, forward)(asset, graph, library):
            raise RuntimeError(f"Could not forward the traversal query on {name}. Discard unsaved changes.")
        if asset_path in dirty_packages():
            package = project.parent / f"Content/Blueprints/{name}.uasset"
            backup = project.parent / "Saved/BlueprintMigration" / backup_group / package.name
            if backup.exists() and backup.read_bytes() != package.read_bytes():
                raise RuntimeError(f"Traversal query backup differs from the current package: {backup}")
            pending.append((asset, package, backup))

    assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
    for asset, package, backup in pending:
        backup.parent.mkdir(parents=True, exist_ok=True)
        if not backup.exists():
            shutil.copy2(package, backup)
        if not editor.compile_blueprint(asset) or not assets.save_loaded_asset(asset, only_if_is_dirty=False):
            raise RuntimeError(f"Could not install the native traversal query. Previous copy: {backup}")
    unreal.log("Native traversal queries installed." if pending else "Native traversal queries already installed.")


if __name__ == "__main__":
    migrate()
