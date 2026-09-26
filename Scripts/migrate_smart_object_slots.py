"""Transfer slot and NPC behavior to native StateTree nodes while retaining bindings."""

from pathlib import Path
import shutil

import unreal


ROOT = "/Game/Blueprints/SmartObjects/TasksAndConditions"
NPC_ROOT = "/Game/Blueprints/AI/StateTree/TasksAndConditions"
NATIVE_PACKAGE = "/Script/GameAnimationSampleCpp"
TASK_PARENT = "/Script/StateTreeModule.StateTreeTaskBlueprintBase"
CONDITION_PARENT = "/Script/StateTreeModule.StateTreeConditionBlueprintBase"
EVALUATOR_PARENT = "/Script/StateTreeModule.StateTreeEvaluatorBlueprintBase"
CONTROLLER_PARENT = "/Script/AIModule.AIController"
TASKS = {
    f"{ROOT}/STT_FindSmartObject": "SampleFindSmartObjectTask",
    f"{ROOT}/STT_ClaimSlot": "SampleClaimSmartObjectTask",
    f"{ROOT}/STT_ClearHandle": "SampleReleaseSmartObjectTask",
    f"{ROOT}/STT_UseSmartObject": "SampleUseSmartObjectTask",
    f"{ROOT}/STT_AddCooldown": "SampleAddInteractionCooldownTask",
    f"{NPC_ROOT}/STT_FindRandomLocation": "SampleFindPatrolLocationTask",
    f"{NPC_ROOT}/STT_FocusToTarget": "SampleFocusInteractionTargetTask",
    f"{NPC_ROOT}/STT_ClearFocus": "SampleClearInteractionFocusTask",
    f"{NPC_ROOT}/STT_FindSlotTransforms": "SampleFindSlotTransformTask",
    f"{NPC_ROOT}/STT_CharacterIgnoreCollisionsWithOtherActor": "SampleSetInteractionCollisionTask",
    f"{NPC_ROOT}/STT_SetCharacterInputState": "SampleSetNpcInputTask",
}
CONDITIONS = {f"{ROOT}/STC_CheckCooldown": "SampleCheckInteractionCooldownCondition"}
EVALUATORS = {f"{ROOT}/STE_GetAIData": "SampleNpcControllerEvaluator"}
CONTROLLERS = {"/Game/Blueprints/AI/AIC_NPC_SmartObject": "SampleNpcController"}

NODE_GROUPS = (
    (TASKS, TASK_PARENT, "EventGraph", "ReceiveLatentEnterState"),
    (CONDITIONS, CONDITION_PARENT, "ReceiveTestCondition", None),
    (EVALUATORS, EVALUATOR_PARENT, "EventGraph", "ReceiveTreeStart"),
    (CONTROLLERS, CONTROLLER_PARENT, "EventGraph", "ReceiveBeginPlay"),
)


def migrate():
    descriptor = Path(unreal.Paths.get_project_file_path()).resolve()
    if descriptor.name != "GameAnimSampleCpp.uproject":
        raise RuntimeError("Run the migration in the native project.")
    editor = unreal.BlueprintEditorLibrary
    assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
    dirty = {package.get_name() for package in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()}
    pending = []
    nodes = ((path, name, parent, graph, event)
             for group, parent, graph, event in NODE_GROUPS for path, name in group.items())
    for asset_path, class_name, expected_parent, graph_name, event in nodes:
        asset = unreal.load_asset(asset_path)
        native = unreal.load_class(None, f"{NATIVE_PACKAGE}.{class_name}")
        current = editor.get_blueprint_parent_class(asset) if asset else None
        if not current or not native:
            raise RuntimeError(f"Cannot load the task or native parent: {asset_path}")
        if current == native:
            continue
        if current.get_path_name() != expected_parent or asset_path in dirty or not editor.can_reparent_blueprint(asset, native):
            raise RuntimeError(f"Node is dirty or its parent differs from the reference: {asset_path}")
        graph = next((graph for graph in editor.list_graphs(asset) if graph.get_name() == graph_name), None)
        if not graph:
            raise RuntimeError(f"No {graph_name} graph for {asset_path}")
        package = descriptor.parent / f"Content/{asset_path.removeprefix('/Game/')}.uasset"
        backup = descriptor.parent / "Saved/BlueprintMigration/SmartObjectSlots" / package.name
        if backup.exists() and backup.read_bytes() != package.read_bytes():
            raise RuntimeError(f"Task backup differs from the current package: {backup}")
        pending.append((asset, graph, native, package, backup, event))

    for asset, graph, native, package, backup, event in pending:
        backup.parent.mkdir(parents=True, exist_ok=True)
        if not backup.exists():
            shutil.copy2(package, backup)
        if event:
            if not unreal.SampleBlueprintMigrationLibrary.remove_isolated_event_override(asset, graph, event):
                raise RuntimeError(f"Cannot isolate the node entry graph: {asset.get_path_name()}")
        else:
            editor.remove_function_graph(asset, graph.get_name())
        editor.reparent_blueprint(asset, native)
        if editor.get_blueprint_parent_class(asset) != native or not editor.compile_blueprint(asset):
            raise RuntimeError(f"Task failed to compile: {asset.get_path_name()}")
        if not assets.save_loaded_asset(asset, only_if_is_dirty=False):
            raise RuntimeError(f"Could not save task: {asset.get_path_name()}")
    unreal.log("Native Smart Object and NPC tasks installed." if pending else "Smart Object and NPC tasks already migrated.")


if __name__ == "__main__":
    migrate()
