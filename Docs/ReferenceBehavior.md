# Reference behavior and parity criteria

The local Game Animation Sample is a content project. Its descriptor declares no project C++ modules. The existing reference log identifies Unreal Engine 5.9.0 build 60738873 and reports both `GM_Sandbox_C` and `GM_Sandbox_Ragdoll_C` loading. The Content directory has 3,890 .uasset packages, five .umap packages, and three movies. The startup package is `/Game/Levels/DefaultLevel`.

Install the assets into the native checkout or prepare an isolated workspace as described in the [README](../README.md). The maps, Blueprints and other imported packages retain the installed sample's package paths. Author and run comparison changes in the destination while keeping the installed sample read-only.

| Area | Local evidence | Native implementation and parity criterion |
| --- | --- | --- |
| Capsule locomotion | `SandboxCharacter_CMC`, `IMC_Sandbox` | The sample pawn uses native gait selection, braking, pre-movement rotation, and handlers for movement, look, and crouch. The input intents for walk, sprint, strafe and aim still use the reference replication path. |
| Mover and ragdoll | `SandboxCharacter_Mover`, `SandboxCharacter_Mover_Ragdoll`, `MovementModes`, `RagdollLevel` | Reference avatars and modes remain. Compare slide entry/exit, transitions through air, ragdoll start, recovery, and movement prediction. |
| Animation selection | CMC and Mover Animation Blueprints and Chooser/Pose Search assets | The native snapshot provides inputs for an authored graph. Validate pose-search selections, trajectory, blending, root motion, locomotion layering, and IK in an integrated graph. |
| Traversal and paired actions | `AC_TraversalLogic`, traversal actions and montage notify states | Native geometry probe covers a facing surface, reachable top and standing clearance. Compare traversal variants, aborts, montage alignment, moving targets, and blend-out after integration. |
| Cameras and input devices | `Blueprints/Cameras`, `IA_Look_Gamepad`, `TIS_MobileControls` and `IMC_Sandbox` | Reference assets remain. Check camera modes, gamepad, keyboard/mouse, touch, aiming, twin-stick and camera collision against the reference. |
| NPCs and Smart Objects | `NPCLevel`, StateTrees, Smart Object claim/use tasks and Bench assets | Reference behavior remains. Test patrol, claim contention, paired animation and cleanup after interruption. |
| Retargeting and presentation | Retargeted character Blueprints, MetaHumans, camera rigs, widgets and audio | Retain skeletal data and authored graph assets. Test pawn/visual switching, retargeted poses, HUD/debug controls, foley and foot placement. |
| Isolated examples | `LocomotorLevel`, `LookAtPOI_Level`, `IsolatedExamples` | Reference behavior remains. Verify each example's controls and transitions and its travel back to the sandbox. |

The copied sample continues to run its gameplay Blueprints for most systems. The two animation tick dispatchers now execute in the project runtime module. Pawn and animation parity still require wiring the remaining native behavior and comparing it under equivalent inputs.

The isolated workspace built its Win64 Development Editor target. In unattended `UnrealEditor-Cmd.exe -game -NullRHI` smoke runs, all five level worlds started for play and exited with code zero. A content-only control run of `DefaultLevel` reproduced the 39 widget Blueprint compiler messages in the native workspace with no difference in the compiler messages. Headless startup verifies package loading and world initialization. Rendered gameplay and widget behavior are **Not run**.

The direct checkout import matched all 3,898 files in the source `Content/` tree by SHA-256 and started all five worlds in unattended Game mode. After shortening the descriptor filename, both Win64 targets built again and the interactive Editor opened `DefaultLevel` from the checkout.

SHA-256 comparison matched all 3,899 files under the installed sample's `Content/` and `Build/` directories with the isolated workspace. On this machine, selecting `-CompilerVersion=14.44.35207` avoided a missing linker library in another installed toolchain.

## Retained authoring

- Animation Blueprints, Pose Search, Chooser, curves, IK rigs, retargeters and Control Rigs encode authored pose behavior and dependencies. Preserve their interfaces while migrating reusable calculations.
- Gameplay Camera rigs, widget layout, sound data, level geometry, input actions and mapping contexts stay authorable. Move reusable runtime orchestration to native code only after verifying the graph's runtime inputs and outputs.
- The existing pawn, controller, traversal and NPC gameplay graphs remain in the local workspace. The portable plugin currently implements the interfaces documented in the [migration guide](Migration.md).

## Blueprint migration acceptance

The local Asset Registry lists 127 Blueprint assets, 20 Animation Blueprints, 15 Control Rig Blueprints, four runtime Widget Blueprints, ten Editor Utility Widget Blueprints and one Editor Utility Blueprint. It also lists five worlds, four StateTrees and 15 Chooser Tables. These are package class counts. Track executable graphs and references per asset to determine migration coverage.

Track each executable graph by its owning asset, callers, entry point, inputs, authored defaults and observable effects. A native implementation counts as adopted when the real map, pawn, component, notify or task invokes it and the superseded Blueprint execution path is removed. A successful compile, dependency load or map launch is recorded as a separate check.

Player and NPC behavior need separate coverage. When replacing a generated class, inventory placed instances, component templates, child classes, serialized references and any bound delegates before changing the class. Preserve the package path and compatible data during the transition. Track temporary Blueprint subclasses that retain bindings as dependencies until the callers migrate.

Audit tick groups and prerequisites for character movement and animation. Verify pose queries, StateTree decisions, root motion, motion warping and IK at the same update phase. Test cancellation and cleanup for interrupted montages, lost targets, unpossession and level travel. Run identical inputs in the reference and destination and record observable results such as state transitions, montage sections and selected poses.

Keep animation clips, skeletons, retargeters, Pose Search data, authored pose graphs, blending, animation playback decisions and rig evaluation in Unreal assets. Move all other runtime logic into C++, including input handling, character state, movement, traversal, and player and NPC interaction requests, eligibility, execution and cleanup. Feed native character state into the retained animation graphs. Preserve the graph interfaces, update phase and authored tuning. Record each migrated entry point and any non-animation logic still executed by a Blueprint.

### Animation tick dispatch

`AC_PreCMCTick` and `AC_PostABPTick` each used a Receive Tick event to invoke a zero-parameter dispatcher named `Tick`. `USampleAnimationTickComponent` implements that dispatch. The migration preserves the two component packages, tick settings, dispatcher signatures and existing bindings. The two packages remain Blueprint subclasses so the existing callers can continue resolving their serialized component references and delegate bindings.

Build the Editor target and close the destination Editor before running the migration. From the repository root, use the `$project` and `$editor` paths from the [README](../README.md).

```powershell
$script = (Resolve-Path Scripts/migrate_animation_tick_components.py).Path
& $editor $project -run=PythonScript "-script=$script" -Unattended -NullRHI
& $editor $project -Unattended -NullRHI '-ExecCmds=Automation RunTests GameAnimationSampleCpp.Migration.AnimationTickDispatch' '-TestExit=Automation Test Queue Empty'
```

The migration checks both assets before editing and backs up the original local packages under `Saved/BlueprintMigration/TickDispatch/`. Keep those backups until subscribers and component instances have been migrated to native types. The automation test loads each saved component class, checks that Receive Tick has no Blueprint bytecode, and verifies that the native tick calls an attached listener.

On UE 5.9.0 build 60738873, the migration saved two packages with matching tick settings. The rerun wrote zero packages. `GameAnimationSampleCpp.Migration.AnimationTickDispatch` passed for both saved classes. Tick-order parity with reference character movement and rendered animation parity are **Not run**.

### Camera shake animation notify

`BP_AnimNotify_SimpleCameraShake` used two graphs to provide its display name and start a radial camera shake at the mesh owner's location. `USampleCameraShakeAnimNotify` implements both entry points with the observed 500 cm and 1,000 cm radii, falloff of 1, and camera-local orientation. The notify package retains the authored shake class and the references from animation tracks.

Run the migration after building the Editor target and closing the destination Editor.

```powershell
$script = (Resolve-Path Scripts/migrate_camera_shake_notify.py).Path
& $editor $project -run=PythonScript "-script=$script" -Unattended -NullRHI
& $editor $project -Unattended -NullRHI '-ExecCmds=Automation RunTests GameAnimationSampleCpp.Migration' '-TestExit=Automation Test Queue Empty'
```

The script backs up the original local package under `Saved/BlueprintMigration/CameraShake/` and verifies the authored class after compiling. On UE 5.9.0 build 60738873, the second migration run made no edits. `GameAnimationSampleCpp.Migration.CameraShakeNotify` passed with the original animation notify package path and native execution. It verified shake scales of 1, 0.5 and 0 at distances of 100, 750 and 2,000 cm. Rendered camera comparison is **Not run**.

### Level teleporter

`ASampleLevelTeleporter` replaces the overlap event, construction script and label function in `Teleporter_Level`. The original package retains its authored mesh, trigger, text component and destination. Existing references and per-instance destination overrides continue to resolve through that package. After validating the entering actor and destination, the native actor requests `/Game/Levels/<destination>?listen` from the authoritative world.

Build the Editor target and close the destination Editor. Run the migration and its automation test from the repository root.

```powershell
$script = (Resolve-Path Scripts/migrate_level_teleporter.py).Path
& $editor $project -run=PythonScript "-script=$script" -Unattended -NullRHI
& $editor $project -Unattended -NullRHI '-ExecCmds=Automation RunTests GameAnimationSampleCpp.Migration.LevelTeleporter' '-TestExit=Automation Test Queue Empty'
```

The script backs up the original local package under `Saved/BlueprintMigration/LevelTeleporter/`. On UE 5.9.0 build 60738873, `GameAnimationSampleCpp.Migration.LevelTeleporter` passed. It verified the authored components, label, and queued travel URL through the overlap delegate, and checked that a missing map or actor queues no travel. Rendered transitions and multiplayer client travel are **Not run**.

### CMC movement and gait

The local `SandboxCharacter_CMC` asset inherits from `ASampleCmcCharacter`. Its existing pre-movement dispatcher calls native rotation and movement updates. The movement update selects gait from raw Enhanced Input action values and the existing walk and sprint intent, then sets acceleration, braking deceleration, ground friction, standing speed and crouch speed. The class samples both movement actions through its possession input component, so direction handling and analog thresholds stay independent of input normalization.

The Blueprint continues to own authored values and serialize `Gait` and `CharacterInputState` for the existing animation interfaces. Input events still manage those intents and their replication. The `DefaultSlot` montage gate remains authored.

Build the Editor target and close the destination Editor. The script accepts the stock reference character or a character with the earlier movement migration already applied. If your local copy only has the older crouch-speed migration, use its stock package backup under `Saved/BlueprintMigration/CmcCrouchSpeed/` as the starting file. Run the script from the repository root.

```powershell
$script = (Resolve-Path Scripts/migrate_cmc_gait.py).Path
& $editor $project -run=PythonScript "-script=$script" -Unattended -NullRHI
```

The script verifies authored defaults and saves a pre-change package in `Saved/BlueprintMigration/CmcGait/<starting stage>/`. To roll back locally, close the destination Editor and restore that package to `Content/Blueprints/SandboxCharacter_CMC.uasset` with the matching code revision. The original installed sample is a separate project.

UE 5.9.0 build 60738873 compiled both Win64 Development targets. The script installed from both supported starting packages with zero errors, and a rerun preserved the installed package hash. All six `GameAnimationSampleCpp.Migration` tests passed. The gait test injects raw camera-relative and world-relative Enhanced Input values into a possessed copy of the saved character. It invokes the original pre-CMC event and verifies gait, braking, and speed updates. The CMC and Mover Animation Blueprints match the original packages by SHA-256. Rendered, multiplayer and packaged comparisons are **Not run**.

### CMC movement, look, and crouch input

After the gait migration, five `Triggered` callbacks are installed during local input setup. Camera-relative movement uses the controller's yaw and the authored stick mode. World-relative movement stays normalized on the world axes. Mouse look supplies a per-update delta and gamepad look applies the world's frame delta to the turn rate. Crouch continues to toggle outside falling. The existing mapping context, default actions, jump and traversal callbacks, input intent events, and animation packages remain in place.

With the destination Editor closed, run the input migration and the complete migration suite.

```powershell
$script = (Resolve-Path Scripts/migrate_cmc_input.py).Path
& $editor $project -run=PythonScript "-script=$script" -Unattended -NullRHI
& $editor $project -Unattended -NullRHI '-ExecCmds=Automation RunTests GameAnimationSampleCpp.Migration' '-TestExit=Automation Test Queue Empty'
```

The script replaces isolated input event chains, enables native bindings on the local pawn class, and saves a pre-change package under `Saved/BlueprintMigration/CmcInput/`. Unmigrated pawns still use their existing Blueprint handlers.

Both Win64 Development targets built and all seven migration tests passed on UE 5.9.0 build 60738873. The input test verifies the saved pawn's single handler per migrated trigger, analog camera-relative movement, normalized world-relative movement, mouse and gamepad look, and crouch gating. The independent Smart Object component retains its movement action subscription. The installation script reran without changing the package hash. Rendered and multiplayer input comparisons are **Not run**.

See the [Epic sample documentation](https://dev.epicgames.com/documentation/unreal-engine/game-animation-sample-project-in-unreal-engine) for the sample's public controls and architecture.
