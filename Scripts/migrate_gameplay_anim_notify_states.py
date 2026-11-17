"""Move gameplay notify callbacks to native parents while retaining authored fields."""

import filecmp
from pathlib import Path
import shutil

import unreal


NOTIFIES = (
    (
        "BP_NotifyState_OverrideMovementMode",
        "SampleOverrideMovementModeAnimNotifyState",
        {"Received_NotifyBegin": 7, "Received_NotifyEnd": 11},
        ("OverrideMovementMode", "EndingMovementMode"),
        {"Received_NotifyBegin": ("QueueNextMovementMode",),
         "Received_NotifyEnd": ("QueueNextMovementMode", "GetMovementModeName")},
    ),
    (
        "BP_NotifyState_OverrideCapsuleCollision",
        "SampleOverrideCapsuleCollisionAnimNotifyState",
        {"Received_NotifyBegin": 7, "Received_NotifyEnd": 8},
        ("New Response", "ResetOnEnd"),
        {"Received_NotifyBegin": ("SetCollisionResponsetoAllChannels",),
         "Received_NotifyEnd": ("SetCollisionProfileName",)},
    ),
)

EXPECTED_PIN_DEFAULTS = {
    "QueueNextMovementMode": {"bShouldReenter": "false"},
    "SetCollisionProfileName": {"InCollisionProfileName": "CharacterCapsule", "bUpdateOverlaps": "true"},
}


def authored_defaults(editor, blueprint, fields):
    cdo = unreal.get_default_object(editor.generated_class(blueprint))
    python_names = {
        "OverrideMovementMode": "override_movement_mode",
        "EndingMovementMode": "ending_movement_mode",
        "New Response": "new_response",
        "ResetOnEnd": "reset_on_end",
    }
    values = []
    for field in fields:
        try:
            value = cdo.get_editor_property(field)
        except Exception:
            value = cdo.get_editor_property(python_names[field])
        values.append(value)
    return tuple(values)


def inspect_callbacks(graphs, expected_shape, expected_calls):
    shape = {}
    for name, graph in graphs.items():
        nodes = [
            node for node in unreal.ObjectIterator(unreal.EdGraphNode)
            if node.get_outer() == graph and node.get_class().get_name() != "EdGraphNode_Comment"
        ]
        shape[name] = len(nodes)
        if name not in expected_calls:
            raise RuntimeError(f"Unexpected gameplay notify graph: {name}")
        infos = unreal.BlueprintToolset.get_node_infos(nodes)
        for expected_call in expected_calls[name]:
            matches = [info for info in infos if info.type_id.endswith("|" + expected_call)]
            if len(matches) != 1:
                raise RuntimeError(f"Unexpected gameplay notify dispatch in {name}: {expected_call}")
            pins = {pin.name: pin for pin in matches[0].input_pins}
            for pin_name, value in EXPECTED_PIN_DEFAULTS.get(expected_call, {}).items():
                pin = pins.get(pin_name)
                if not pin or pin.connected_pins or pin.value != value:
                    raise RuntimeError(f"Unexpected authored default in {name}: {expected_call}.{pin_name}")
    if shape != expected_shape:
        raise RuntimeError(f"Unexpected gameplay notify graph shape: {shape}")


def migrate():
    descriptor = Path(unreal.Paths.get_project_file_path()).resolve()
    if descriptor.name != "GameAnimSampleCpp.uproject":
        raise RuntimeError("Run this migration in the native sample project.")

    editor = unreal.BlueprintEditorLibrary
    assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
    pending = []
    for name, parent_name, shape, fields, calls in NOTIFIES:
        asset = f"/Game/Blueprints/AnimNotifies/{name}"
        blueprint = unreal.load_asset(asset)
        parent = unreal.load_class(None, f"/Script/GameAnimationSampleCpp.{parent_name}")
        if not blueprint or not parent:
            raise RuntimeError(f"Import the sample and build the native Editor target first: {name}")
        if any(item.get_name() == asset for item in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()):
            raise RuntimeError(f"Reload the dirty gameplay notify before migration: {name}")
        previous_parent = editor.get_blueprint_parent_class(blueprint)
        graphs = {graph.get_name(): graph for graph in editor.list_graphs(blueprint)}
        defaults = authored_defaults(editor, blueprint, fields)
        if previous_parent == parent and not graphs:
            unreal.log(f"{name} already migrated.")
            continue
        if previous_parent != unreal.AnimNotifyState.static_class() or not editor.can_reparent_blueprint(blueprint, parent):
            raise RuntimeError(f"Unexpected gameplay notify parent: {name}")
        inspect_callbacks(graphs, shape, calls)
        package = descriptor.parent / "Content" / (asset.removeprefix("/Game/") + ".uasset")
        backup = descriptor.parent / "Saved/BlueprintMigration/GameplayAnimNotifyStates" / package.name
        if backup.exists() and not filecmp.cmp(package, backup, shallow=False):
            raise RuntimeError(f"The local package differs from its earlier backup: {name}")
        pending.append((blueprint, parent, graphs, fields, defaults, package, backup))

    # Back up and validate both inputs before modifying either asset. The existing variables
    # stay in the Blueprint so their serialized names, types and montage overrides are retained.
    for _, _, _, _, _, package, backup in pending:
        backup.parent.mkdir(parents=True, exist_ok=True)
        if not backup.exists():
            shutil.copy2(package, backup)
    for blueprint, parent, graphs, fields, defaults, _, backup in pending:
        for graph in graphs.values():
            editor.remove_graph(blueprint, graph)
        editor.reparent_blueprint(blueprint, parent)
        if not editor.compile_blueprint(blueprint):
            raise RuntimeError(f"Gameplay notify compilation failed. Original copy: {backup}")
        if editor.get_blueprint_parent_class(blueprint) != parent or editor.list_graphs(blueprint):
            raise RuntimeError(f"Gameplay notify callback migration is incomplete. Original copy: {backup}")
        if authored_defaults(editor, blueprint, fields) != defaults:
            raise RuntimeError(f"Gameplay notify authored defaults changed. Original copy: {backup}")

    for blueprint, _, _, _, _, _, backup in pending:
        if not assets.save_loaded_asset(blueprint, only_if_is_dirty=False):
            raise RuntimeError(f"Gameplay notify save failed. Original copy: {backup}")
        unreal.log(f"Native gameplay notify installed: {blueprint.get_name()}")


if __name__ == "__main__":
    migrate()
