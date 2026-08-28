"""Move the two sandbox animation tick dispatchers to their native parent."""

from pathlib import Path
import filecmp
import shutil

import unreal


COMPONENTS = ("AC_PreCMCTick", "AC_PostABPTick")
NATIVE_PARENT = "/Script/GameAnimationSampleCpp.SampleAnimationTickComponent"


def entry_nodes(graph):
    return sorted(
        node.get_class().get_name()
        for node in unreal.ObjectIterator(unreal.EdGraphNode)
        if node.get_outer() == graph and node.get_class().get_name() != "EdGraphNode_Comment"
    )


def tick_settings(blueprint):
    cdo = unreal.get_default_object(unreal.BlueprintEditorLibrary.generated_class(blueprint))
    return cdo.get_editor_property("primary_component_tick").export_text()


def migrate():
    descriptor = Path(unreal.Paths.get_project_file_path()).resolve()
    if descriptor.name != "GameAnimSampleCpp.uproject":
        raise RuntimeError("Run this migration in the native sample project.")

    editor = unreal.BlueprintEditorLibrary
    parent = unreal.load_class(None, NATIVE_PARENT)
    if not parent:
        raise RuntimeError("Build the native Editor target before running the migration.")

    pending = []
    for name in COMPONENTS:
        blueprint = unreal.load_asset(f"/Game/Blueprints/{name}")
        if not blueprint:
            raise RuntimeError(f"Local sample content is missing {name}.")
        event_graph = editor.find_event_graph(blueprint)
        previous_parent = editor.get_blueprint_parent_class(blueprint)
        if previous_parent == parent and (not event_graph or not entry_nodes(event_graph)):
            continue

        graphs = sorted(graph.get_name() for graph in editor.list_graphs(blueprint))
        events = sorted(str(event.name) for event in editor.list_events(blueprint) if event.is_implemented)
        if (
            previous_parent != unreal.ActorComponent.static_class()
            or graphs != ["EventGraph", "Tick"]
            or events != ["ReceiveTick"]
            or entry_nodes(event_graph) != ["K2Node_CallDelegate", "K2Node_Event"]
            or not editor.can_reparent_blueprint(blueprint, parent)
        ):
            raise RuntimeError(f"Unexpected graph or parent on {name}. Review the asset before migrating.")
        pending.append((name, blueprint, event_graph, tick_settings(blueprint)))

    backup = descriptor.parent / "Saved" / "BlueprintMigration" / "TickDispatch"
    for name, _, _, _ in pending:
        package = descriptor.parent / "Content" / "Blueprints" / f"{name}.uasset"
        backup.mkdir(parents=True, exist_ok=True)
        destination = backup / package.name
        if destination.exists():
            if not filecmp.cmp(package, destination, shallow=False):
                raise RuntimeError(f"The local package differs from its earlier backup for {name}.")
        else:
            shutil.copy2(package, destination)

    for name, blueprint, event_graph, settings in pending:
        editor.remove_graph(blueprint, event_graph)
        editor.reparent_blueprint(blueprint, parent)
        if not editor.compile_blueprint(blueprint):
            raise RuntimeError(f"Compilation failed for {name}.")
        if tick_settings(blueprint) != settings:
            raise RuntimeError(f"Tick settings changed for {name}.")
        if editor.get_blueprint_parent_class(blueprint) != parent:
            raise RuntimeError(f"Native parent was not installed on {name}.")

    assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
    for name, blueprint, _, _ in pending:
        if not assets.save_loaded_asset(blueprint, only_if_is_dirty=False):
            raise RuntimeError(f"Could not save {name}. Original copies are in {backup}.")
        unreal.log(f"Native animation tick installed for {name}.")

    unreal.log(f"Animation tick migration complete. Updated {len(pending)} packages.")


if __name__ == "__main__":
    migrate()
