"""Install native sandbox style application and block construction."""

from hashlib import sha256
from pathlib import Path
import shutil

import unreal


ROOT = "/Game/Levels/LevelPrototyping"
VISUALS_REFERENCE = "e7bb92765bc35254bc58990a1598700bd0d944ec1fefdae20f6608c920d97bc5"


def migrate():
    descriptor = Path(unreal.Paths.get_project_file_path()).resolve()
    if descriptor.name != "GameAnimSampleCpp.uproject":
        raise RuntimeError("Run the migration in the native project.")
    editor = unreal.BlueprintEditorLibrary
    assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
    dirty = {item.get_name() for item in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()}
    pending = []

    for name, native_name, remove in (
        ("LevelBlock", "SampleLevelBlock", {"UserConstructionScript", "UpdateMaterials"}),
        ("LevelVisuals", "SampleLevelVisuals", {"UserConstructionScript", "UpdateLevelVisuals", "EventGraph"}),
    ):
        path = f"{ROOT}/{name}"
        asset = unreal.load_asset(path)
        native = unreal.load_class(None, f"/Script/GameAnimationSampleCpp.{native_name}")
        if not asset or not native or path in dirty:
            raise RuntimeError(f"Cannot load a clean asset and its native parent: {path}")
        parent = editor.get_blueprint_parent_class(asset)
        graphs = {graph.get_name(): graph for graph in editor.list_graphs(asset)}
        if parent == native and remove.isdisjoint(graphs):
            continue

        package = descriptor.parent / "Content" / (path.removeprefix("/Game/") + ".uasset")
        if not remove <= graphs.keys():
            raise RuntimeError(f"The style graphs differ from the reference: {path}")
        if name == "LevelBlock" and parent != native:
            raise RuntimeError("Run migrate_sandbox_block.py first.")
        if name == "LevelVisuals" and (parent != unreal.Actor.static_class() or
                not editor.can_reparent_blueprint(asset, native) or
                sha256(package.read_bytes()).hexdigest() != VISUALS_REFERENCE):
            raise RuntimeError(f"The style asset differs from the reference: {path}")
        backup = descriptor.parent / "Saved/BlueprintMigration/SandboxVisuals" / package.name
        if backup.exists() and backup.read_bytes() != package.read_bytes():
            raise RuntimeError(f"The style backup differs from the current package: {backup}")
        pending.append((asset, native, parent, [graphs[key] for key in sorted(remove)], package, backup))

    for asset, native, parent, graphs, package, backup in pending:
        backup.parent.mkdir(parents=True, exist_ok=True)
        if not backup.exists():
            shutil.copy2(package, backup)
        for graph in graphs:
            editor.remove_graph(asset, graph)
        if parent != native:
            editor.reparent_blueprint(asset, native)

    for asset, native, _, _, _, backup in pending:
        if editor.get_blueprint_parent_class(asset) != native or not editor.compile_blueprint(asset):
            raise RuntimeError(f"The style asset failed to compile: {asset.get_path_name()}")
        if not assets.save_loaded_asset(asset, only_if_is_dirty=False):
            raise RuntimeError(f"Could not save {asset.get_path_name()}. Previous copy: {backup}")
    unreal.log("Native sandbox visuals installed." if pending else "Sandbox visuals already migrated.")


if __name__ == "__main__":
    migrate()
