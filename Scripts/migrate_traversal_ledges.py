"""Install native spline queries behind the original traversal-block interface."""

from hashlib import sha256
from pathlib import Path
import shutil

import unreal


ASSET = "/Game/Levels/LevelPrototyping/LevelBlock_Traversable"
REFERENCE_HASH = "5d530888fc6db2644b0365c051d9acc799db908d96f3a505855a28e2b4ee95eb"
ENTRY = "GetLedgeTransforms"
OLD_QUERY = "FindLedgeClosestToActor"


def migrate():
    descriptor = Path(unreal.Paths.get_project_file_path()).resolve()
    if descriptor.name != "GameAnimSampleCpp.uproject":
        raise RuntimeError("Run the migration in the native sample project.")
    if any(package.get_name() == ASSET for package in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()):
        raise RuntimeError("The traversal block has unsaved changes.")

    editor = unreal.BlueprintEditorLibrary
    asset = unreal.load_asset(ASSET)
    library = unreal.load_class(None, "/Script/GameAnimationSampleCpp.SampleTraversalLedgeLibrary")
    if not asset or not library:
        raise RuntimeError("Import the reference sample and build the native Editor target.")
    graphs = {graph.get_name(): graph for graph in editor.list_graphs(asset)}
    if ENTRY not in graphs or "UserConstructionScript" not in graphs:
        raise RuntimeError("The traversal block entry point or construction is missing.")
    if OLD_QUERY not in graphs:
        unreal.log("Traversal ledges already migrated.")
        return

    package = descriptor.parent / "Content/Levels/LevelPrototyping/LevelBlock_Traversable.uasset"
    if sha256(package.read_bytes()).hexdigest() != REFERENCE_HASH:
        raise RuntimeError("The traversal block differs from the supported reference.")
    backup = descriptor.parent / "Saved/BlueprintMigration/TraversalLedges" / package.name
    backup.parent.mkdir(parents=True, exist_ok=True)
    if backup.exists() and backup.read_bytes() != package.read_bytes():
        raise RuntimeError(f"An incompatible traversal block backup exists: {backup}")
    if not backup.exists():
        shutil.copy2(package, backup)

    parent = editor.get_blueprint_parent_class(asset)
    migration = unreal.SampleBlueprintMigrationLibrary
    if not migration.replace_void_function_body(asset, graphs[ENTRY], library, "UpdateBlockLedges"):
        raise RuntimeError(f"The native ledge call could not be wired. Previous copy: {backup}")
    editor.remove_graph(asset, graphs[OLD_QUERY])
    if not editor.compile_blueprint(asset):
        raise RuntimeError(f"The traversal block failed to compile. Previous copy: {backup}")
    if editor.get_blueprint_parent_class(asset) != parent:
        raise RuntimeError(f"The block's authored parent changed. Previous copy: {backup}")
    if not unreal.get_editor_subsystem(unreal.EditorAssetSubsystem).save_loaded_asset(asset, only_if_is_dirty=False):
        raise RuntimeError(f"Failed to save the traversal block. Previous copy: {backup}")
    unreal.log("Native traversal ledges installed.")


if __name__ == "__main__":
    migrate()
