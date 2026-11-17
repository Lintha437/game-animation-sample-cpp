"""Install the native camera shake implementation on the sample's local notify."""

import filecmp
from pathlib import Path
import shutil

import unreal


ASSET = "/Game/Blueprints/AnimNotifies/BP_AnimNotify_SimpleCameraShake"
NATIVE_PARENT = "/Script/GameAnimationSampleCpp.SampleCameraShakeAnimNotify"


def graph_node_count(graph):
    return sum(
        node.get_outer() == graph and node.get_class().get_name() != "EdGraphNode_Comment"
        for node in unreal.ObjectIterator(unreal.EdGraphNode)
    )


def migrate():
    descriptor = Path(unreal.Paths.get_project_file_path()).resolve()
    if descriptor.name != "GameAnimSampleCpp.uproject":
        raise RuntimeError("Run this migration in the native sample project.")

    editor = unreal.BlueprintEditorLibrary
    assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
    retirement = descriptor.parent / "Saved/BlueprintMigration/NativeAnimationNotifies/conversion.json"
    if retirement.is_file() and not assets.does_asset_exist(ASSET):
        return
    blueprint = unreal.load_asset(ASSET)
    parent = unreal.load_class(None, NATIVE_PARENT)
    if not blueprint or not parent:
        raise RuntimeError("Import the sample and build the native Editor target first.")

    previous_parent = editor.get_blueprint_parent_class(blueprint)
    graphs = {graph.get_name(): graph for graph in editor.list_graphs(blueprint)}
    if previous_parent == parent and not graphs:
        unreal.log("Camera shake notify already migrated.")
        return

    shape = {name: graph_node_count(graph) for name, graph in graphs.items()}
    if (
        previous_parent != unreal.AnimNotify.static_class()
        or shape != {"Received_Notify": 6, "GetNotifyName": 2}
        or not editor.can_reparent_blueprint(blueprint, parent)
    ):
        raise RuntimeError("Unexpected camera shake notify graph or parent. Review the asset before migrating.")

    # Check the action and all authored constants so a locally customized notify is preserved.
    graph = graphs["Received_Notify"]
    nodes = [
        node for node in unreal.ObjectIterator(unreal.EdGraphNode)
        if node.get_outer() == graph and node.get_class().get_name() != "EdGraphNode_Comment"
    ]
    calls = [
        info for info in unreal.BlueprintToolset.get_node_infos(nodes)
        if info.type_id.endswith("|PlayWorldCameraShake")
    ]
    if len(calls) != 1:
        raise RuntimeError("Unexpected camera shake dispatch.")
    pins = {pin.name: pin for pin in calls[0].input_pins}
    for name, expected in {"InnerRadius": 500, "OuterRadius": 1000, "Falloff": 1}.items():
        if pins[name].connected_pins or float(pins[name].value) != expected:
            raise RuntimeError(f"Unexpected camera shake {name}.")
    if pins["bOrientShakeTowardsEpicenter"].connected_pins or pins["bOrientShakeTowardsEpicenter"].value != "false":
        raise RuntimeError("Unexpected camera shake orientation.")

    cdo = unreal.get_default_object(editor.generated_class(blueprint))
    original_shake = cdo.get_editor_property("shake")
    package = descriptor.parent / "Content" / (ASSET.removeprefix("/Game/") + ".uasset")
    backup = descriptor.parent / "Saved" / "BlueprintMigration" / "CameraShake" / package.name
    backup.parent.mkdir(parents=True, exist_ok=True)
    if backup.exists():
        if not filecmp.cmp(package, backup, shallow=False):
            raise RuntimeError("The local package differs from its earlier backup.")
    else:
        shutil.copy2(package, backup)

    for graph in graphs.values():
        editor.remove_graph(blueprint, graph)
    editor.reparent_blueprint(blueprint, parent)
    if not editor.compile_blueprint(blueprint):
        raise RuntimeError("Camera shake notify compilation failed.")
    cdo = unreal.get_default_object(editor.generated_class(blueprint))
    if cdo.get_editor_property("shake") != original_shake:
        raise RuntimeError("The authored camera shake class changed.")
    if not assets.save_loaded_asset(blueprint, only_if_is_dirty=False):
        raise RuntimeError(f"Camera shake notify save failed. Original copy: {backup}")
    unreal.log("Native camera shake notify installed.")


if __name__ == "__main__":
    migrate()
