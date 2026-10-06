"""Install native visual override loading and child actor updates."""

from hashlib import sha256
from pathlib import Path
import shutil

import unreal


ASSET = "/Game/Blueprints/AC_VisualOverrideManager"
REFERENCE = "e6f20daf50d5149d6c318446a67e23d5bad9fe94de8ee490d73ef776506afed7"


def migrate():
    descriptor = Path(unreal.Paths.get_project_file_path()).resolve()
    if descriptor.name != "GameAnimSampleCpp.uproject":
        raise RuntimeError("Run the migration in the native project.")
    editor = unreal.BlueprintEditorLibrary
    asset = unreal.load_asset(ASSET)
    native = unreal.load_class(None, "/Script/GameAnimationSampleCpp.SampleVisualOverrideComponent")
    dirty = {item.get_name() for item in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()}
    if not asset or not native or ASSET in dirty:
        raise RuntimeError("Cannot load a clean visual override component and its native parent.")
    parent = editor.get_blueprint_parent_class(asset)
    if parent == native:
        unreal.log("Visual override already migrated.")
        return

    graphs = {graph.get_name(): graph for graph in editor.list_graphs(asset)}
    package = descriptor.parent / "Content" / (ASSET.removeprefix("/Game/") + ".uasset")
    if (parent != unreal.ActorComponent.static_class() or not editor.can_reparent_blueprint(asset, native)
            or set(graphs) != {"FindAndApplyVisualOverride", "OnRep_VisualOverride", "ApplyVisualOverride", "EventGraph"}
            or sha256(package.read_bytes()).hexdigest() != REFERENCE):
        raise RuntimeError("The visual override component differs from the reference.")
    backup = descriptor.parent / "Saved/BlueprintMigration/VisualOverride" / package.name
    if backup.exists() and sha256(backup.read_bytes()).hexdigest() != REFERENCE:
        raise RuntimeError(f"The visual override backup differs from the reference: {backup}")
    backup.parent.mkdir(parents=True, exist_ok=True)
    if not backup.exists():
        shutil.copy2(package, backup)

    for name in ("EventGraph", "FindAndApplyVisualOverride", "ApplyVisualOverride"):
        editor.remove_graph(asset, graphs[name])
    editor.reparent_blueprint(asset, native)
    if not unreal.SampleBlueprintMigrationLibrary.replace_mover_input_function(
        asset, graphs["OnRep_VisualOverride"], "ApplyVisualOverride", "None"
    ):
        raise RuntimeError("Could not forward the existing replication notification.")
    if editor.get_blueprint_parent_class(asset) != native or not editor.compile_blueprint(asset):
        raise RuntimeError("The visual override component failed to compile.")
    assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
    if not assets.save_loaded_asset(asset, only_if_is_dirty=False):
        raise RuntimeError(f"Could not save the visual override component. Previous copy: {backup}")
    unreal.log("Native visual override installed.")


if __name__ == "__main__":
    migrate()
