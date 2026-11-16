"""Move the authored Look At About panel execution into its existing native Widget parent."""

import filecmp
from pathlib import Path
import shutil

import unreal


ASSET = "/Game/IsolatedExamples/LookAtPOI/TestLevelContent/Widget/WB_LookAtPOI__GUI"


def migrate():
    descriptor = Path(unreal.Paths.get_project_file_path()).resolve()
    project_root = descriptor.parent
    if descriptor.name != "GameAnimSampleCpp.uproject":
        raise RuntimeError("Run this migration in the existing native Game Animation Sample checkout.")

    package = project_root / "Content" / (ASSET.removeprefix("/Game/") + ".uasset")
    backup = project_root / "Saved/BlueprintMigration/LookAtAboutPanel" / package.name
    if not package.is_file():
        raise RuntimeError(f"Missing Look At widget package: {package}")

    editor = unreal.BlueprintEditorLibrary
    asset = unreal.load_asset(ASSET)
    native = unreal.load_class(None, "/Script/GameAnimationSampleCpp.SampleLookAtInterface")
    dirty_packages = {item.get_name() for item in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()}
    if not asset or not native or ASSET in dirty_packages:
        raise RuntimeError("The Look At widget must be loaded, clean, and have its compiled native parent available.")
    if editor.get_blueprint_parent_class(asset) != native:
        raise RuntimeError("The previous native gameplay-button migration must already be installed.")

    graph = editor.find_event_graph(asset)
    if not graph:
        raise RuntimeError("Look At widget is missing its original EventGraph.")
    migration = unreal.SampleBlueprintMigrationLibrary
    if migration.is_look_at_about_panel_migrated(asset, graph):
        unreal.log("Look At About panel already structurally migrated. No asset changes.")
        return

    if backup.is_file() and not filecmp.cmp(package, backup, shallow=False):
        raise RuntimeError(f"Existing About-panel backup differs from the current pre-migration widget: {backup}")
    backup.parent.mkdir(parents=True, exist_ok=True)
    if not backup.exists():
        shutil.copy2(package, backup)

    if not migration.forward_look_at_about_pages(asset, graph):
        raise RuntimeError(f"Could not preserve the three About page tails. Backup: {backup}")
    if not migration.remove_isolated_event_override(asset, graph, "Construct"):
        raise RuntimeError(f"Could not isolate the original Construct body. Backup: {backup}")
    if not migration.remove_look_at_about_events(asset, graph):
        raise RuntimeError(f"Could not isolate the original About/Close handlers. Backup: {backup}")

    if not editor.compile_blueprint(asset):
        raise RuntimeError(f"Look At About-panel widget failed to compile. Backup: {backup}")
    if not migration.is_look_at_about_panel_migrated(asset, graph):
        raise RuntimeError("Post-compile native call, localization, media, and obsolete-handler verification failed.")
    assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
    if not assets.save_loaded_asset(asset, only_if_is_dirty=False):
        raise RuntimeError(f"Could not save Look At About-panel widget. Backup: {backup}")

    unreal.log("Look At About panel migrated, compiled, and saved with all three authored page branches.")


if __name__ == "__main__":
    migrate()
