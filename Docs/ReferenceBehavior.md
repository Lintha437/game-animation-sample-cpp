# Reference behavior and parity criteria

The local Game Animation Sample is a content project. Its descriptor declares no project C++ modules. The existing reference log identifies Unreal Engine 5.9.0 build 60738873 and reports both `GM_Sandbox_C` and `GM_Sandbox_Ragdoll_C` loading. The Content directory has 3,890 .uasset packages, five .umap packages, and three movies. The startup package is `/Game/Levels/DefaultLevel`.

Install the assets into the native checkout or prepare an isolated workspace as described in the [README](../README.md). The maps, Blueprints and other imported packages retain the installed sample's package paths. Author and run comparison changes in the destination while keeping the installed sample read-only.

| Area | Local evidence | Native implementation and parity criterion |
| --- | --- | --- |
| Capsule locomotion | `SandboxCharacter_CMC`, `IMC_Sandbox` | The sample pawn uses native gait selection, braking, pre-movement rotation, movement and look handlers, crouch input, and replicated input intent. The existing character state property still supplies the authored animation and camera interfaces. |
| Mover and ragdoll | `SandboxCharacter_Mover`, `SandboxCharacter_Mover_Ragdoll`, `MovementModes`, `RagdollLevel` | The primary Mover pawn uses native mode and gait queries, horizontal speed, and sandbox teleport. Compare slide, input production, ragdoll transitions, recovery, and movement prediction. |
| Animation selection | CMC and Mover Animation Blueprints and Chooser/Pose Search assets | The native snapshot provides inputs for an authored graph. Validate pose-search selections, trajectory, blending, root motion, locomotion layering, and IK in an integrated graph. |
| Traversal and paired actions | `AC_TraversalLogic`, traversal actions and montage notify states | Native geometry probe covers a facing surface, reachable top and standing clearance. Compare traversal variants, aborts, montage alignment, moving targets, and blend-out after integration. |
| Cameras and input devices | `Blueprints/Cameras`, `IA_Look_Gamepad`, `TIS_MobileControls` and `IMC_Sandbox` | Reference assets remain. Check camera modes, gamepad, keyboard/mouse, touch, aiming, twin-stick and camera collision against the reference. |
| NPCs and Smart Objects | `NPCLevel`, StateTrees, Smart Object claim/use tasks and Bench assets | Reference behavior remains. Test patrol, claim contention, paired animation and cleanup after interruption. |
| Retargeting and presentation | Retargeted character Blueprints, MetaHumans, camera rigs, widgets and audio | Retain skeletal data and authored graph assets. Test pawn/visual switching, retargeted poses, HUD/debug controls, foley and foot placement. |
| Isolated examples | `LocomotorLevel`, `LookAtPOI_Level`, `IsolatedExamples` | Reference behavior remains. Verify each example's controls and transitions and its travel back to the sandbox. |

The native module handles the sample's movement, input, traversal clearance and movement modes, ragdoll state, Smart Object requests and collision, and several scenario controls. Imported Blueprint packages retain the serialized interfaces and call into the native module at the migrated entry points. The sections below record runtime checks, authored callbacks, and remaining comparisons.

The isolated workspace built its Win64 Development Editor target. In unattended `UnrealEditor-Cmd.exe -game -NullRHI` smoke runs, all five level worlds started for play and exited with code zero. A content-only control run of `DefaultLevel` reproduced the 39 widget Blueprint compiler messages in the native workspace with no difference in the compiler messages. Headless startup verifies package loading and world initialization. Rendered gameplay and widget behavior are **Not run**.

The direct checkout import matched all 3,898 files in the source `Content/` tree by SHA-256 and started all five worlds in unattended Game mode. After shortening the descriptor filename, both Win64 targets built again and the interactive Editor opened `DefaultLevel` from the checkout.

SHA-256 comparison matched all 3,899 files under the installed sample's `Content/` and `Build/` directories with the isolated workspace. On this machine, selecting `-CompilerVersion=14.44.35207` avoided a missing linker library in another installed toolchain.

## Retained authoring

| Assets | Logic retained and reason |
| --- | --- |
| Main Animation Blueprints, Pose Search and Chooser Tables, IK rigs, retargeters, Control Rig, and MetaHuman setup | Pose selection, blending, rig evaluation, retargeting, eye setup, and binding to authored character data remain with the animation stack. |
| `AC_TraversalLogic` | The CMC and Mover montage nodes, their phase callbacks and the Choosers consume the native clearance and action result. The component supplies runtime warp targets. The original typed RPC declarations preserve the Blueprint struct payload and network flags. |
| `AC_SmartObjectAnimation`, `STT_PlayAnimFromBestCost`, and `STT_PlayAnimMontage` | Montage evaluation and cost gates, start time, looping, warp targets, and completion callbacks stay with the authored interaction. The StateTree assets retain their bindings and state transitions. |
| `SandboxCharacter_Mover_Ragdoll` and Mover/CMC pawn interface graphs | Motion Match Multi, rolling get-up selection, rig physics profiles and authored pose data remain editable. The pawn interfaces preserve the existing Blueprint struct signatures. Camera and traversal queries forward to native code. |
| Gameplay Camera rigs, foley, widget layouts, and timeline obstacles | Camera rigs, sound recipes, material settings, UI layouts and the authored timing of obstacle playback stay editable. This includes `SpinningArrow` and the proximity-driven Bumper and SmackArm timelines. |

### Remaining runtime graph audit

The remaining Blueprint audit includes the sandbox input display (`WB_SimpleController`), debug data collection in animation and AI graphs, and Look At POI presentation and level-script setup. The local Asset Registry found no game-package referencer for `WB_SimpleController`. A literal scan of 3,895 asset and map packages in each project found the controller widget name in its own package and in `GameAnimationWidget`. The latter declares `EditorUtilityWidget` as its native parent. Both packages are absent from the reference archive that contains all five gameplay maps. The Editor graph shows that the controller overlay toggle reaches `UpdateControllerWidget` in the utility widget. That function obtains a PIE world and its player controller, creates `WB_SimpleController`, and adds it to the player screen. The utility can also remove the widget.

In the native project's Standalone PIE session, invoking the utility button's existing delegate outside an Editor transaction changed the visible overlay count from zero to one and back to zero. The stored widget had the local player controller as its owner and left the viewport after the second toggle. PIE teardown completed with no transaction reference error or crash.

Standalone PIE runs in both projects clicked the actual `Controller` button in the `GameAnimationWidget` utility. Each run changed the visible overlay count from zero to one and back to zero. The local `PC_Sandbox` controller owned the overlay while it was shown, and the same stored widget left the viewport after the second click. Both PIE worlds tore down without a garbage-collection failure, transaction reference error, or new crash directory. Physical keyboard, mouse and gamepad response sampling is **Not run**.

The live Editor graph audit verified that `AIC_NPC_SmartObject` has native parent `ASampleNpcController`. Its startup timer and StateTree dispatch are implemented by the native parent. A separate graph export shows an empty Event Graph and a `Debug` function that draws the controlled pawn's forward direction. The saved native package still contains the `Debug`, `DrawDebugArrow` and `MakeVisualLoggerDebugDrawer` names. The audit verified that the Construction Script entry is unconnected. Runtime callers of the `Debug` function remain to be verified. The saved native packages for the five Blueprint tasks in its two StateTrees declare the expected native parents. These tasks use SampleSetNpcInputTask, SampleFindSmartObjectTask, SampleClaimSmartObjectTask, SampleUseSmartObjectTask and SampleFindPatrolLocationTask. The StateTrees retain their authored task order and engine-native Move To and Delay tasks. Live pawn possession, debug invocation and StateTree task execution remain pending.

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

The Blueprint continues to own authored values and serialize `Gait` and `CharacterInputState` for the existing animation interfaces. The `DefaultSlot` montage gate remains authored.

Build the Editor target and close the destination Editor. The script accepts the stock reference character or a character with the earlier movement migration already applied. If your local copy only has the older crouch-speed migration, use its stock package backup under `Saved/BlueprintMigration/CmcCrouchSpeed/` as the starting file. Run the script from the repository root.

```powershell
$script = (Resolve-Path Scripts/migrate_cmc_gait.py).Path
& $editor $project -run=PythonScript "-script=$script" -Unattended -NullRHI
```

The script verifies authored defaults and saves a pre-change package in `Saved/BlueprintMigration/CmcGait/<starting stage>/`. To roll back locally, close the destination Editor and restore that package to `Content/Blueprints/SandboxCharacter_CMC.uasset` with the matching code revision. The original installed sample is a separate project.

UE 5.9.0 build 60738873 compiled both Win64 Development targets. The script installed from both supported starting packages with zero errors, and a rerun preserved the installed package hash. All six `GameAnimationSampleCpp.Migration` tests passed. The gait test injects raw camera-relative and world-relative Enhanced Input values into a possessed copy of the saved character. It invokes the original pre-CMC event and verifies gait, braking, and speed updates. The CMC and Mover Animation Blueprints match the original packages by SHA-256. Rendered, multiplayer and packaged comparisons are **Not run**.

### CMC movement, look, and crouch input

After the gait migration, five `Triggered` callbacks are installed during local input setup. Camera-relative movement uses the controller's yaw and the authored stick mode. World-relative movement stays normalized on the world axes. Mouse look supplies a per-update delta and gamepad look applies the world's frame delta to the turn rate. Crouch continues to toggle outside falling. The existing mapping context, default actions, jump and traversal callbacks, and animation packages remain in place.

With the destination Editor closed, run the movement input migration.

```powershell
$script = (Resolve-Path Scripts/migrate_cmc_input.py).Path
& $editor $project -run=PythonScript "-script=$script" -Unattended -NullRHI
```

The script replaces isolated input event chains, enables native bindings on the local pawn class, and saves a pre-change package under `Saved/BlueprintMigration/CmcInput/`. Unmigrated pawns still use their existing Blueprint handlers.

Both Win64 Development targets built and all seven migration tests passed on UE 5.9.0 build 60738873. The input test verifies the saved pawn's single handler per migrated trigger, analog camera-relative movement, normalized world-relative movement, mouse and gamepad look, and crouch gating. The independent Smart Object component retains its movement action subscription. The installation script reran without changing the package hash. Rendered and multiplayer input comparisons are **Not run**.

### CMC input intent

Walk and strafe toggle on press. Sprint clears the walk selection and, like aim, responds to both press and release. The local pawn updates the same `CharacterInputState` property consumed by the authored camera and animation interfaces. The owning client submits a typed snapshot through an unreliable Server RPC, matching the reference event's flags. The server updates the existing replicated property, including compatibility with push-model replication. The `Set_CharacterInputState` interface remains available to existing callers.

After the movement input migration, run the input intent migration and the complete suite.

```powershell
$script = (Resolve-Path Scripts/migrate_cmc_input_intent.py).Path
& $editor $project -run=PythonScript "-script=$script" -Unattended -NullRHI
& $editor $project -Unattended -NullRHI '-ExecCmds=Automation RunTests GameAnimationSampleCpp.Migration' '-TestExit=Automation Test Queue Empty'
```

The migration compares the old and new network flags before editing, checks the original defaults after compilation, and backs up the previous pawn package under `Saved/BlueprintMigration/CmcInputIntent/`.

On UE 5.9.0 build 60738873, both Win64 Development targets built, the migration installed with zero errors, and a rerun preserved its package hash. The seven-test migration suite passed. The CMC input test exercises press and release, walk and sprint priority, strafe toggling, preservation of crouch intent, and authoritative writes to the existing replicated state.

A two-player listen-server PIE session in `NPCLevel` verified the aim action over the client-to-server path. Both pawns were CMC. The remote player's client and its counterpart on the server reported false before press, true while held, and false after release. Packet loss and a separate observing client are **Not run**.

### Sandbox pawn selection and restart

`ASampleSandboxGameMode` selects the pawn for login and restart. It reads the level's existing `PawnClasses_Soft` and default pawn class. Changes to `DDCvar.PawnClass` preload the new choice and replace possessed player pawns at their current transforms with their aim restored. A newer request cancels the previous load, and teardown releases the request and delegate. Controllers without a pawn continue through the normal spawn flow. A failed or abstract selection leaves existing pawns in place.

The `GM_Sandbox` and `GM_Sandbox_Ragdoll` packages still supply the level-specific defaults. With the destination Editor closed, run the local GameMode migration and the migration suite.

```powershell
$script = (Resolve-Path Scripts/migrate_sandbox_game_mode.py).Path
& $editor $project -run=PythonScript "-script=$script" -Unattended -NullRHI
& $editor $project -Unattended -NullRHI '-ExecCmds=Automation RunTests GameAnimationSampleCpp.Migration' '-TestExit=Automation Test Queue Empty'
```

The migration verifies the stock GameMode package and backs it up under `Saved/BlueprintMigration/SandboxGameMode/`.

Both Win64 Development targets built on UE 5.9.0 build 60738873. The migration installed with zero errors and reran without changing the package hash. All eight migration tests passed, including CMC and Mover selection and fallback to the sandbox and ragdoll defaults. Two-player PIE in `NPCLevel` switched both players from Mover to CMC and back, including possession on the remote client. Horizontal positions and the observed zero aim yaw were retained. Vertical positions differed by up to 7.3 cm during replacement. Nonzero aim yaw and packaged checks are **Not run**.

### Sandbox player controller

`ASampleSandboxPlayerController` handles the three authored sandbox input actions on the owning controller. The gaze teleport sweeps a 32 cm sphere on the same trace channel as the reference and moves the pawn with the reference 50 cm clearance on hit. The authored maximum distance moves to the native class defaults. With a clear view the pawn moves to the sweep endpoint. The same operation handles CMC and Mover, using the original local execution and teleport physics.

Pawn and visual cycling retain their original server delivery. The pawn change is reliable and the visual change is unreliable. The native GameMode advances the existing console variables and authored lists, including index wrapping. The visual data and its consumers remain asset-driven. Android and iOS retain the joystick-hide behavior when a gamepad is assigned.

After applying the GameMode migration, close the destination Editor and run the controller migration.

```powershell
$script = (Resolve-Path Scripts/migrate_sandbox_controller.py).Path
& $editor $project -run=PythonScript "-script=$script" -Unattended -NullRHI
& $editor $project -Unattended -NullRHI '-ExecCmds=Automation RunTests GameAnimationSampleCpp.Migration' '-TestExit=Automation Test Queue Empty'
```

The local script backs up both affected packages under `Saved/BlueprintMigration/SandboxController/`. It preserves the two original network flag sets and leaves the authored game mode and controller package paths in place.

Both Win64 Development targets built and all nine migration tests passed on UE 5.9.0 build 60738873. The controller test verifies unobstructed range, the sphere-hit clearance, pawn rotation, and the two RPC policies. Installing and rerunning the migration produced zero errors, and the rerun preserved both package hashes. In two-player `NPCLevel` PIE, the remote input advanced the visual index from -1 to 0 and switched the pawn from CMC to Mover. Host input teleported each pawn roughly 100 m in open space. Mobile joystick behavior, packet-loss cases and packaged runs are **Not run**.

### Traversal movement hooks

The local `AC_TraversalLogic` package uses `USampleTraversalComponent` for its CMC movement mode and replication updates. The animation event graph still starts the montage, selects its authored warp targets, and invokes the existing CMC hooks in order. The CMC path enters flying and temporarily enables its client-position flags. Blend-out or interruption invokes the exit mode for the selected action, then restores normal correction after the original 0.2-second delay. The Mover path keeps its original mode queue and flags. Collision management and the traversal eligibility checks remain in the current Blueprint.

Build the Editor target, close the destination Editor, and apply the local migration.

```powershell
$script = (Resolve-Path Scripts/migrate_traversal_movement.py).Path
& $editor $project -run=PythonScript "-script=$script" -Unattended -NullRHI
& $editor $project -Unattended -NullRHI '-ExecCmds=Automation RunTests GameAnimationSampleCpp.Migration' '-TestExit=Automation Test Queue Empty'
```

The migration backs up the previous package under `Saved/BlueprintMigration/TraversalMovement/`. The original traversal action data, montage chooser, warp targets, CMC and Mover playback graphs, and their package references stay authored.

Both Win64 Development targets built and all ten migration tests passed on UE 5.9.0 build 60738873. The test covers the four authored exit choices, both CMC flags, and the Mover guard. The package hash stayed unchanged on rerun. The event, traversal selection and warp target graphs matched their pre-migration DSL. Direct calls on a possessed CMC pawn in `NPCLevel` PIE verified walking → flying → walking and both flag changes. An injected montage did not start the traversal event path, so montage interruption, obstacle-driven playback and multiplayer correction remain unverified.

### Traversable block ledges

`USampleTraversalLedgeLibrary` selects and samples the obstacle's authored splines. It chooses the nearest facing edge from the pawn position, applies the minimum width and corner clearance at the impact point, then samples the mapped opposite edge. The local `LevelBlock_Traversable` keeps its authored parent, construction, and typed `GetLedgeTransforms` signature. The existing traversal component still supplies the trace hit and consumes the same by-reference result.

Build the Editor target, close the destination Editor, and apply the local migration.

```powershell
$script = (Resolve-Path Scripts/migrate_traversal_ledges.py).Path
& $editor $project -run=PythonScript "-script=$script" -Unattended -NullRHI
& $editor $project -Unattended -NullRHI '-ExecCmds=Automation RunTests GameAnimationSampleCpp.Migration' '-TestExit=Automation Test Queue Empty'
```

The script backs up the reference package under `Saved/BlueprintMigration/TraversalLedges/`. A rerun preserved the installed package hash, and the source package matched its original hash. Both Win64 Development targets built and all eleven migration tests passed on UE 5.9.0 build 60738873. The ledge test calls the saved Blueprint entry point and verifies its in-place result, corner clearance, opposite edge sampling, preservation of unrelated result fields, and rejection of a short ledge. The local `DefaultLevel` loaded 28 traversable block instances in the editor. Curved and rotated splines, physical obstacle playback, and multiplayer correction are **Not run**.

### Traversal obstacle and clearance sweeps

`FSampleTraversalClearanceQuery` performs the approach sweep, front-edge clearance, over-obstacle sweep, and far-floor sweep. It keeps the reference's capsule sizes, trace channel indices, initial-overlap rejection on the front sweep, corner clearance, and far-floor reach. An overhead hit invalidates the back ledge and reports depth to the hit. The imported `AC_TraversalLogic.TryTraversalAction` calls the native query and passes the same typed reference result to the existing CMC and Mover Choosers. The asset still evaluates pose history, selects and plays montages, sets warp targets, and dispatches the traversal events.

Build the Editor target, close the destination Editor, apply the traversal movement migration, and then apply the clearance migration.

```powershell
$script = (Resolve-Path Scripts/migrate_traversal_clearance.py).Path
& $editor $project -run=PythonScript "-script=$script" -Unattended -NullRHI
& $editor $project -Unattended -NullRHI '-ExecCmds=Automation RunTests GameAnimationSampleCpp.Migration' '-TestExit=Automation Test Queue Empty'
```

The script saves the previous local package under `Saved/BlueprintMigration/TraversalClearance/`. Both Win64 Development targets built, and all eleven migration tests passed. The expanded ledge test verifies an actual capsule hit, obstacle height and depth, a clear far floor, an overhead blockage, and a blocked front approach. A rerun preserved the migrated package hash. The original package matched its reference hash.

In single-player `DefaultLevel` PIE, `LevelBlock_Traversable36` triggered `AM_M_Neutral_Traversal_Vault_1_0_stand_F_Lfoot` through the original traversal function on CMC and Mover. Both pawns crossed the obstacle and returned to ground, and CMC movement correction flags returned to their normal values. The Mover run supplied the approach direction explicitly. Montage interruption, the normal Mover input chain, and multiplayer correction are **Not run**.

### Traversal activity and collision

`USampleTraversalComponent` sets the existing activity field at the authored montage callback sites. The native collision scope suspends the capsule response to the struck component, and the Mover path also suspends the mesh response to `WorldDynamic`. Finish and interrupted callbacks release those changes, and component teardown calls the same cleanup. The scope preserves ignore entries owned by other systems and restores the mesh response captured at entry.

Build the Editor target, close the destination Editor, and apply the local migration after the traversal movement migration.

```powershell
$script = (Resolve-Path Scripts/migrate_traversal_collision.py).Path
& $editor $project -run=PythonScript "-script=$script" -Unattended -NullRHI
& $editor $project -Unattended -NullRHI '-ExecCmds=Automation RunTests GameAnimationSampleCpp.Migration' '-TestExit=Automation Test Queue Empty'
```

The migration backs up the local package under `Saved/BlueprintMigration/TraversalCollision/`. The authored montage calls, warp targets, movement mode changes and CMC correction delay remain in their existing order. Both Win64 Development targets built and all eleven migration tests passed. The extended test covers duplicate starts, retargeting, repeated cleanup and preservation of host-owned collision settings.

In single-player `DefaultLevel` PIE, CMC and Mover cleared traversal activity and the obstacle ignore entry after normal vault playback and after an explicit interruption. The Mover mesh returned from Ignore to Block. CMC returned to Walking and restored the two client correction flags. In the Mover test that interrupted playback in the same frame as the start, collision recovered but the pawn stayed in Traversing. The retained Mover exit checks the current mode while the start queues its transition for the next simulation frame. Verify that case when migrating Mover mode ownership. The Mover tests supplied the approach direction. Multiplayer correction and teardown during level travel are **Not run**.

### Traversal movement mode lifetime

The montage callbacks now call `BeginCmcTraversalMode`, `FinishCmcTraversalMode`, `BeginMoverTraversalMode`, and `FinishMoverTraversalMode`. CMC keeps the reference exit mode and 0.2-second correction grace period. The component cancels an earlier correction timer at the next traversal start and clears it on teardown. Mover cancels a same-step pending start and preserves the current mode. Once Traversing is active, it queues Walking on exit.

Build the Editor target, close the destination Editor, and apply the local migration after the traversal collision migration.

```powershell
$script = (Resolve-Path Scripts/migrate_traversal_modes.py).Path
& $editor $project -run=PythonScript "-script=$script" -Unattended -NullRHI
& $editor $project -Unattended -NullRHI '-ExecCmds=Automation RunTests GameAnimationSampleCpp.Migration' '-TestExit=Automation Test Queue Empty'
```

The local package is backed up under `Saved/BlueprintMigration/TraversalModes/`. Both Win64 Development targets built and all eleven migration tests passed, including the native CMC entry and grace period. A rerun preserved the installed package hash. In single-player `DefaultLevel` PIE, interrupting the selected Mover vault in the start frame kept the pawn grounded. Interrupting after observing Traversing restored the ground mode and collision. The Mover test supplied the approach direction. CMC kept both flags immediately after interruption, cleared them after the timer, and returned to Walking once grounded.

In two-player listen-server PIE on `DefaultLevel`, an owning CMC client initiated the vault. At an uncapped frame rate, the client and authority finished in Walking with both flags cleared and positions within 0.01 cm. At `t.MaxFPS 3`, both remained Flying four game seconds after the start despite clearing the flags. The saved pre-mode-migration Blueprint reproduced the failure under the same frame cap and also landed in the uncapped control. See [CMC traversal network recovery](#cmc-traversal-network-recovery) for the subsequent fix.

### Traversal replay recipients

The imported server and multicast declarations keep their original network flags and traversal payload. The multicast calls `USampleTraversalComponent::ApplyTraversalReplay`. The authority for a remote pawn and simulated copies install the typed result and invoke the authored playback event. The locally controlled pawn keeps its predicted playback. Receipt of the result during an active CMC traversal restores Flying if an earlier movement correction interrupted that mode.

Build the Editor target, close the destination Editor, and apply the local replay migration.

```powershell
$script = (Resolve-Path Scripts/migrate_traversal_replay.py).Path
& $editor $project -run=PythonScript "-script=$script" -Unattended -NullRHI
& $editor $project -Unattended -NullRHI '-ExecCmds=Automation RunTests GameAnimationSampleCpp.Migration' '-TestExit=Automation Test Queue Empty'
```

The local package is backed up under `Saved/BlueprintMigration/TraversalReplay/`. Both Win64 Development targets built and all eleven migration tests passed. The tests exercise local ownership gating and the original RPC directions. In two-player listen-server PIE at 60 FPS, the owning client and authority each started the selected vault exactly once. A host-initiated run likewise started once on the host and once on the observer. Both runs reached the lower platform and returned to Walking with the flags cleared, and the two copies in each run settled within 0.01 cm. At 3 FPS, the owning client and authority still remained Flying after four game seconds even though both flags cleared and each montage started once. See [CMC traversal network recovery](#cmc-traversal-network-recovery) for the subsequent fix.

### Mover state and sandbox teleport

`ASampleMoverCharacter` reads the current mode and velocity from the existing Mover component. The authored name map still defines the mode enumeration. Gait uses the pre-simulation input snapshot. Sprint takes precedence over the walk toggle, aim caps the gait at Run, and strafe uses the original dot-product and style thresholds. The existing Blueprint query signatures continue to return the original enum types. The native teleport handles absolute and relative moves, queues Falling, and updates the cached facing and optional reset transform.

Build the Editor target, close the destination Editor, and apply the migration to the imported Mover pawn.

```powershell
$script = (Resolve-Path Scripts/migrate_mover_state.py).Path
& $editor $project -run=PythonScript "-script=$script" -Unattended -NullRHI
& $editor $project -Unattended -NullRHI '-ExecCmds=Automation RunTests GameAnimationSampleCpp.Migration' '-TestExit=Automation Test Queue Empty'
```

The script backs up the original pawn under `Saved/BlueprintMigration/MoverState/`. The mode map stayed unchanged, and a rerun preserved the package hash. Both Win64 Development targets built and all twelve migration tests passed. The new test exercises the typed gait query, sprint precedence, aim and strafe gates, analog thresholds, and absolute and relative teleport orientation.

In `DefaultLevel` PIE, the possessed Mover pawn returned the original grounded, traversing and airborne enum values. Its reported speed matched the component's horizontal velocity during the authored vault. The pawn completed the vault and returned to ground, and a separate teleport updated its reset transform before entering Falling. The test supplied the vault direction. The original pawn, the copied Animation Blueprint, and the test map passed hash checks. Full Mover inputs, slide, ragdoll and multiplayer prediction are **Not run** for this migration.

### Mover input command pipeline

The original `ProduceInput` interface forwards to `ASampleMoverCharacter::BuildMoverInputCommand`. C++ updates the default and custom blocks in their authored order and returns an independent copy of each block in the command. The original device, orientation and direction queries feed those blocks, and the authored direction curves still determine the animation-facing offset. The native pre-simulation callback reads crouch from the current command. The native cache copies the last simulated input blocks to the existing animation and camera snapshots. Targeted aim, twin-stick aim selection, and normalized control yaw rate also execute in C++.

Build the Editor target, close the destination Editor, and apply the migration after the Mover state migration.

```powershell
$script = (Resolve-Path Scripts/migrate_mover_input_pipeline.py).Path
& $editor $project -run=PythonScript "-script=$script" -Unattended -NullRHI
& $editor $project -Unattended -NullRHI '-ExecCmds=Automation RunTests GameAnimationSampleCpp.Migration' '-TestExit=Automation Test Queue Empty'
```

The script saves the prior local pawn under `Saved/BlueprintMigration/MoverInputPipeline/`. Both Win64 Development targets built and all twelve migration tests passed. The Mover test invokes the saved interface and verifies both command blocks, directional intent, the jump edge and crouch intent. In `DefaultLevel` PIE, the owning Mover client advanced with injected `IA_Move` and captured its direction in the post-simulation snapshot. `IA_Jump` produced the jump edge and an airborne transition before returning to ground. Releasing the crouch state through the authored action updated the client's and authority's simulated inputs and stance. Twin-stick, simulated packet loss, and the ragdoll pawn are **Not run**.

### Mover facing and floor state

The primary Mover pawn builds its facing intent in C++ using the current simulation mode, input direction, cached facing, and based movement rotation. The native floor query updates the fields consumed by the authored camera and animation graphs. The post-finalize hook dispatches the existing mode-change callback once per mode transition. The animation graph and its authored curves remain in their existing packages.

Apply this step after the Mover input command migration. The script backs up the prior local pawn under `Saved/BlueprintMigration/MoverOrientation/`.

```powershell
$script = (Resolve-Path Scripts/migrate_mover_orientation.py).Path
& $editor $project -run=PythonScript "-script=$script" -Unattended -NullRHI
& $editor $project -Unattended -NullRHI '-ExecCmds=Automation RunTests GameAnimationSampleCpp.Migration.MoverState' '-TestExit=Automation Test Queue Empty'
```

Both Win64 Development targets built. The focused Mover state test passed and verifies directional facing on the ground and how a stationary pawn follows a moving base. In `DefaultLevel` PIE, the possessed Mover pawn reported the grounded mode, forward facing, and a floor normal of (0, 0, 1). The Mover Animation Blueprint matches the original package by SHA-256. Slide and multiplayer prediction are **Not run** for this migration.

### Ragdoll simulation inputs

The ragdoll pawn extends the native Mover command with a copy of its pelvis and spine transform when the queued mode is Ragdoll. The capsule facing follows the authored freeze and rolling flags. C++ measures center-of-mass and spine velocities, applies the authored fall-speed gravity limit, and checks whether a pawn can initiate or receive a character interaction. The imported Blueprint still drives the trigger, profile transitions, recovery montage, and paired animation.

Apply the migration after the primary Mover migrations. The script saves the prior local pawn under `Saved/BlueprintMigration/RagdollState/`.

```powershell
$script = (Resolve-Path Scripts/migrate_ragdoll_state.py).Path
& $editor $project -run=PythonScript "-script=$script" -Unattended -NullRHI
```

Both Win64 Development targets built. In `RagdollLevel` PIE the possessed pawn transitioned from Walking to Ragdoll and back to Walking. The ragdoll command transform and spine velocity changed during simulation. Target eligibility was false during ragdoll and was restored on recovery. The smoke check called the existing trigger and exit events. Raw input and multiplayer are **Not run**.

### Ragdoll entry and recovery

The native pawn activates skeletal physics, queues entry or exit, confirms entry on the following engine frame, and maintains the original quarter-second heading freeze. During ragdoll the capsule ignores pawns. Recovery restores capsule collision before the authored pose snapshot and schedules restoration of the default Physics Control profile after the montage handoff. The imported pawn retains the injury input, pose history, get-up chooser and playback.

Apply this after installing the ragdoll state migration. The script saves the previous local pawn under `Saved/BlueprintMigration/RagdollLifecycle/`.

```powershell
$script = (Resolve-Path Scripts/migrate_ragdoll_lifecycle.py).Path
& $editor $project -run=PythonScript "-script=$script" -Unattended -NullRHI
```

Both Win64 Development targets built. In `RagdollLevel` PIE, entry requested the Ragdoll profile immediately, the heading freeze cleared, and the finalized Ragdoll state ignored pawn collision. Recovery returned to Walking, restored pawn blocking, and reapplied PhysicalAnimation. Entry followed by an exit request in the same frame rolled back to the default profile and unfroze the capsule. Multiplayer and raw input are **Not run**.

### Ragdoll obstacle checks and physics profiles

The native pawn invokes the authored Physics Control recipe, switches collision between the original leg body sets, and selects the joint constraint profile based on the active physics state. A 15 cm sweep from the neck to the midpoint between the feet detects the Obstacle object type and invokes the existing ragdoll reaction.

After the lifecycle migration, install the profile and obstacle hooks.

```powershell
$script = (Resolve-Path Scripts/migrate_ragdoll_physics.py).Path
& $editor $project -run=PythonScript "-script=$script" -Unattended -NullRHI
```

Both Win64 Development targets built. In `RagdollLevel` PIE, a clear sweep kept the pawn in Walking. Moving an existing obstacle into the sweep triggered the Ragdoll profile immediately. The pawn entered Ragdoll, then returned to Walking and PhysicalAnimation after recovery. The check observed the applied profile and capsule responses. Individual physics contact pairs are **Not run**.

### Smart Object slot search, claim, and release

The native StateTree task parents take over the three entry points while retaining the imported task assets and their existing bindings. Search uses the actor's spatial context and Gameplay Interaction behavior filter, then publishes the selected slot and owner. The claim step publishes a normal-priority claim and leaves completion to the interaction step. Cleanup releases the supplied claim. The StateTree flow and bench playback assets remain in the sample content.

```powershell
$script = (Resolve-Path Scripts/migrate_smart_object_slots.py).Path
& $editor $project -run=PythonScript "-script=$script" -Unattended -NullRHI
```

The script saves the three original task packages under `Saved/BlueprintMigration/SmartObjectSlots/`.

Both Win64 Development targets built. In `NPCLevel` PIE, the native task instances produced valid candidate slots and five valid claims across Mover and CMC NPCs. The observed slots reported both Occupied and Free over the run. Forced same-slot contention and interrupted interaction cleanup are **Not run**.

### NPC patrol, approach, and cooldown

The same installer connects patrol sampling, focus, slot transforms, collision requests, and the cooldown condition to native StateTree nodes. The patrol sampler keeps the authored navigation radius and Smart Object exclusion distance. It tries at most 16 points and falls back to the actor's position, so a fully excluded area can finish its update. Collision requests keep using the existing paired-interaction component's replicated event. The cooldown condition's label stays in the condition asset.

The Win64 Editor target built and the updated tasks compiled in the migration. In `NPCLevel` PIE, five native patrol tasks produced targets and an NPC controller recorded a `Sit` cooldown. Player slot-transform output, multiplayer collision, and a standalone Game build are **Not run** for this batch.

### Smart Object use and paired collision

The native use task dispatches the engine's Gameplay Interaction for the claimed slot. The AI controller owns the engine task after dispatch. The imported component continues to expose the original multicast, montage selection, and playback. C++ applies temporary capsule movement ignores and changes CMC correction on the same peers as the imported behavior.

Run the updated `Scripts/migrate_smart_object_slots.py` installer, then apply the interaction component hooks below.

```powershell
$script = (Resolve-Path Scripts/migrate_smart_object_interaction.py).Path
& $editor $project -run=PythonScript "-script=$script" -Unattended -NullRHI
```

The Win64 Editor target built and both installers compiled their updated assets. In `NPCLevel` PIE, NPCs reached the Occupied slot state and the existing collision event added and restored a CMC capsule's ignore entry. Release of the same slot was unobserved during the smoke run. Multiplayer correction and standalone Game are **Not run**.

### Player Smart Object requests

The native interaction component caches the owning pawn's mesh and movement component. On the authored interact input, it initializes the player StateTree, traces for a usable Smart Object, filters for the nearest free slot, and submits the original event payload. The playback gate still opens in the imported component after an accepted request. The StateTree and its approach, sit, exit, and animation choices remain authored assets.

Apply the native interaction component migration before installing the player request.

```powershell
$script = (Resolve-Path Scripts/migrate_player_smart_object.py).Path
& $editor $project -run=PythonScript "-script=$script" -Unattended -NullRHI
```

The Win64 Editor target built, and the installer compiled the interaction component. In `NPCLevel` PIE, the player request rejected a bench from behind and accepted a bench from the front. The StateTree was running, and its player task resolved the slot transform at (430, 35, 0). This smoke check called the request directly. Enhanced Input dispatch, multiplayer, and full get-up playback are **Not run**.

### Mover device input

The migrated pawn binds its authored Enhanced Input actions in C++ on local possession. The movement query normalizes device input in the control-rotation frame and consumes navigation velocity for AI. Mouse and gamepad look retain their different time scaling. Walk, sprint, strafe, aim, and crouch write the existing intent snapshot. The typed rotation mode query retains the target, aim and twin-stick priorities.

```powershell
$script = (Resolve-Path Scripts/migrate_mover_device_input.py).Path
& $editor $project -run=PythonScript "-script=$script" -Unattended -NullRHI
& $editor $project -Unattended -NullRHI '-ExecCmds=Automation RunTests GameAnimationSampleCpp.Migration.MoverDeviceInput' '-TestExit=Automation Test Queue Empty'
```

The installer backs up the local pawn under `Saved/BlueprintMigration/MoverDeviceInput/`. The Win64 Editor target built and the focused test passed. It possessed the migrated pawn and injected move, strafe, and aim actions through Enhanced Input. It verified control-yaw movement and aim precedence. Multiplayer, analog gamepad view, and standalone Game are **Not run** for this batch.

### Mover jump and traversal input

The native press handler checks movement state, the existing montage slot, and traversal activity. It builds the ground or air approach for the authored traversal call. A rejected probe requests uncrouch when applicable or publishes the one-frame Mover jump edge. Held input can retry the traversal probe. Grounded ragdoll presses request the existing recovery flow.

```powershell
$script = (Resolve-Path Scripts/migrate_mover_jump.py).Path
& $editor $project -run=PythonScript "-script=$script" -Unattended -NullRHI
```

The Win64 Editor built and both pawn packages compiled. In `DefaultLevel` PIE, the authored Enhanced Input action produced an unobstructed jump with a 110 cm rise and returned to Walking. From the approach to `LevelBlock_Traversable_C_4`, the same action entered Traversing, advanced 196 cm during the sample, and returned to Walking. The test placed the pawn at the obstacle approach. Multiplayer and the ragdoll-specific jump binding are **Not run**.

### CMC jump and traversal input

The native CMC pawn checks the original default montage slot and builds the ground or air approach at the jump input boundary. It attempts traversal first on press and invokes the normal CMC jump if geometry or montage selection fails. The CMC and Mover paths now use the same adapter to call the authored traversal component.

```powershell
$script = (Resolve-Path Scripts/migrate_cmc_jump.py).Path
& $editor $project -run=PythonScript "-script=$script" -Unattended -NullRHI
```

The Win64 Editor built and the CMC pawn compiled. In `DefaultLevel` PIE, the authored Enhanced Input action jumped 117.5 cm on clear ground and returned to Walking. From the approach to `LevelBlock_Traversable_C_4`, it entered Flying for the vault, moved 195.7 cm, and eventually restored Walking, cleared traversal activity, and restored CMC correction. The test supplied the obstacle approach. Multiplayer and standalone Game are **Not run** for this batch.

### CMC ragdoll and ledge transitions

The original ragdoll functions forward to native CMC entry and recovery. Entry sets movement to None, disables capsule collision, and simulates bodies below the pelvis. Recovery changes the mesh to Pawn and QueryOnly as the source graph does, restores capsule collision, and resumes Falling. The native ledge callback ends crouch.

```powershell
$script = (Resolve-Path Scripts/migrate_cmc_ragdoll.py).Path
& $editor $project -run=PythonScript "-script=$script" -Unattended -NullRHI
```

The Win64 Editor built and the pawn package compiled. In `DefaultLevel` PIE, the original function calls drove Walking, None, Falling, and Walking in order. Pelvis simulation was active only during ragdoll, and recovery restored capsule collision. Calling the ledge callback while crouched returned the pawn to a standing stance. Multiplayer and physical ledge geometry are **Not run** for this batch.

### Mover targeting and stick updates

The Mover pawn's target interface forwards registration and removal to C++. The native update selects the nearest valid candidate and publishes it to the existing camera and animation readers. The sample's debug marker uses the original cone style. C++ also updates twin-stick aim and the asymmetrically smoothed move deflection from the already bound actions.

```powershell
$script = (Resolve-Path Scripts/migrate_mover_targeting.py).Path
& $editor $project -run=PythonScript "-script=$script" -Unattended -NullRHI
& $editor $project -Unattended -NullRHI '-ExecCmds=Automation RunTests GameAnimationSampleCpp.Migration.MoverDeviceInput' '-TestExit=Automation Test Queue Empty'
```

The Win64 Editor built and the focused test passed. It verified target registration and removal through the imported interface, nearest-target fallback, the twin-stick heading, and both deflection rates. Rendered marker appearance and multiplayer are **Not run** for this batch.

### NPC startup and input routing

The native AI controller schedules the authored StateTree component with the original two-second or eight-second startup delay. The native evaluator resolves the AI controller from the bound actor. The navigation task writes walk intent to the original CMC or Mover input snapshot and clears the other intents as the authored task did. Other pawn implementations can still receive the original Blueprint interface call.

Re-run `Scripts/migrate_smart_object_slots.py` to install the controller, evaluator, and input task.

The Win64 Editor built and the three migrated assets compiled. In `NPCLevel` PIE, five controllers started their original StateTrees, five evaluators populated their AI controller output, and both CMC and Mover pawns reflected the sampled walk requests. Dedicated-server startup timing and standalone Game are **Not run** for this batch.

### NPC debug forwarding

The debug installer compiled and saved `AIC_NPC_SmartObject` with its existing `Debug` entry forwarding to the development-only native implementation. The saved package and the authored backup retained their SHA-256 hashes on repeat installation. The caller inventory has yet to identify an automatic runtime caller, so the verification invoked `Debug` explicitly on the placed controllers.

Before and after installation, five controllers in `NPCLevel` possessed their expected pawns in both Standalone and dedicated-server PIE. The five StateTrees in each mode were inactive at the early check and running at the late check. Each profile invoked the public `Debug` entry on all five controllers. A separate Standalone fixture set control rotation to `(20°, 90°, 0°)` and actor rotation to `(0°, 0°, 0°)`, then invoked `Debug` with and without a pawn. An unpossessed call to the authored graph raised an access warning. The installed forward completed the unpossessed probe without that exception.

The Visual Logger comparison isolated four new shape records under the same NPC fixture and `Draw Debug Library` category by subtracting the authored records from the cumulative session and checking the PIE timestamps against the installation receipt. The four shapes, including all six line segments, matched the authored serialized shape bytes. The rotated shaft endpoint matched the control-rotation calculation within `0.000001 cm`. With no pawn, both recordings contained a shaft from `(0, 0, 0)` to `(50, 0, 0)`. Scene rendering still requires a separate comparison.

### Mover falling and slide transitions

The falling mode keeps the stock linear movement proposal and uses the command's facing and authored offset to calculate angular velocity. The imported assets retain their movement settings and native engine simulation. C++ now evaluates both slide transitions against the original proposed horizontal speed and crouch state.

```powershell
$script = (Resolve-Path Scripts/migrate_mover_falling_and_transitions.py).Path
& $editor $project -run=PythonScript "-script=$script" -Unattended -NullRHI
```

The Win64 Editor built and the three imported assets compiled. In `DefaultLevel` PIE, Enhanced Input jump entered Falling and returned to Walking. The pawn reached 411 cm/s before crouching, entered Sliding, and dropped back to Walking as speed decayed. Isolated air turn rate and multiplayer prediction are **Not run** for this batch.

### Mover walking and sliding

The walking and sliding assets use native smooth-walking subclasses with their existing tuning. Walking selects gait and crouch speed, handles root-motion and slope-relative velocity, and adjusts braking and turn response. Sliding applies the native entry boost and slope tuning while retaining the engine solver. The walking landing flag and slide boost retain their original latent timing.

Run the updated `Scripts/migrate_mover_falling_and_transitions.py` to apply the two ground modes. The Win64 Editor built and both packages compiled. In `DefaultLevel` PIE, sprint used the authored 585 cm/s speed cap. Jump recovery observed the 20,000 cm/s² landing brake and subsequent flag expiry. Sliding entered with its boost, remained active after the boost expired, and returned to Walking. Slope comparisons and multiplayer prediction are **Not run**.

See the [Epic sample documentation](https://dev.epicgames.com/documentation/unreal-engine/game-animation-sample-project-in-unreal-engine) for the sample's public controls and architecture.

### Mover flying and ragdoll

The Flying asset retains stock Mover simulation and uses native damping for linear velocity and facing. The Ragdoll asset uses the command's pelvis transform to follow the ragdoll, applies ground clearance from the downward sphere trace, and writes the capsule transform and velocities to the sync state. A missing pelvis snapshot holds the capsule for that step. The original animation assets still drive the recovery pose and get-up.

Run `Scripts/migrate_mover_falling_and_transitions.py` to install both modes. The Win64 Editor built and both assets compiled. In `RagdollLevel` PIE, Enhanced Input moved the pawn in Flying mode. The native Ragdoll mode aligned the capsule horizontally with the pelvis and kept the capsule above it on the floor. Recovery returned to Walking. Flight trajectory comparison and multiplayer prediction are **Not run** for this batch.

### Mover interaction candidates

The ragdoll pawn polls for eligible characters in a 200 cm overlap, checks the 50 cm height tolerance and 60-degree facing half-angle, and rejects blocked paths using the `CharacterCapsule` profile. The resulting actor list still feeds the authored paired pose search and montage logic.

Run `Scripts/migrate_ragdoll_state.py` to forward the imported candidate poll to C++. The Win64 Editor built and the asset compiled. In `RagdollLevel` PIE, the original tick published a nearby forward candidate and cleared the list when the initiator entered Flying. Direct calls also rejected targets behind the pawn, above the height tolerance, and outside the overlap. Blocked-path and paired montage playback comparisons are **Not run** for this batch.

### Sandbox obstacles and zero gravity

Run `Scripts/migrate_sandbox_obstacles.py` to install the native initialization and overlap handlers. The three source packages matched the reference project before migration. The installer preserves the authored components and obstacle timelines and saves the previous packages under `Saved/BlueprintMigration/SandboxObstacles/`.

The Win64 Editor built and all three assets compiled. In `RagdollLevel` PIE, both physics obstacles used their path controller as the Mover input producer. All 18 timeline instances matched their configured play rates and loop flags, and each looping instance set to start automatically was playing. Entering the zero-gravity zone set both Mover gravity and the ragdoll multiplier to zero. Exiting restored −980 cm/s² and a multiplier of 1. Exact delayed-start timing and a standalone Game build are **Not run** for this batch.

### Intra-level teleporters and targeting dummies

Run `Scripts/migrate_sandbox_fixtures.py` to install the native actors while retaining the imported meshes, trigger shapes, and instance references. The source packages matched the reference project. A missing destination leaves overlap travel idle and produces the missing-destination label.

The Win64 Editor unity build passed. All three migrated assets and the dependent level button compiled. In `DefaultLevel` PIE, all 15 senders had a destination. The tested sender displayed its configured label and moved the pawn to the exact arrival location with yaw unchanged. Entering and leaving the tested dummy trigger added and removed it from the Mover pawn's candidate list. Visual material comparison, networked teleport, and standalone Game are **Not run** for this batch.

### Sandbox level button

The fixture installer moves the level button's construction settings, overlap cooldown, manual press, and console-command dispatch to C++. It preserves the authored `ButtonPressed` delegate and instance settings for existing level bindings.

The Win64 Editor unity build passed, and the imported button compiled. In `DefaultLevel` PIE, two immediate overlaps emitted one delegate broadcast, the enabled command updated a test console variable, and a manual press still broadcast during the cooldown. A later overlap broadcast again after the cooldown. The test restored the console variable and removed its temporary listener. Multiplayer and standalone Game are **Not run**.

### Sandbox block offsets

Run `Scripts/migrate_sandbox_block.py` to forward the existing randomize, reset, and text entry points to C++. The installer preserves the authored offset ranges, button binding, material selection, and traversal child assets.

The Win64 Editor unity build passed and the imported block compiled. `DefaultLevel` PIE found 61 instances of the native block or its Blueprint subclasses. On the tested block, the original randomize event applied a fixed 35 cm test offset without accumulating it across presses. The original reset event restored the starting location. The native label displayed `2.5 M` for a scale of 2.5. No blocks in that level had a randomize button assigned during the run. A bound-button level and standalone Game are **Not run**.

### Sandbox visual themes

Run `Scripts/migrate_sandbox_visuals.py` after the block offset migration. Native construction creates the dynamic grid materials, captures block baselines, and binds the existing randomize delegate. Native style updates apply the imported palette, fog, decal and landscape settings. The `GetLevelStyle` Blueprint accessor remains available for consumers of the imported struct type.

The Win64 Editor unity build passed, and both assets compiled. In `DefaultLevel` PIE, all 61 blocks had dynamic materials. Requests below and above the style array bounds selected the first and last styles respectively. Fog density, decal color, and all 61 block grid colors matched both authored styles. Independent block palettes, landscape material values, fog color, and button rebinding are **Not run** for this batch.

### Sandbox character visual override

Run `Scripts/migrate_visual_override.py` to install the native selection and child actor update. The existing component and class property keep their authored references and replication settings. The native component cancels pending requests when the selection changes and keeps the original mesh available when no override is selected.

The Win64 Editor unity build passed, and the component compiled. In `DefaultLevel` PIE, console selections installed Echo and Manny on the same Mover pawn and hid the source mesh. Clearing the selection removed the child actor class and restored mesh visibility. These classes were already loaded during the smoke check. Cold asynchronous loading, replicated delivery, and other pawn variants are **Not run**.

### Locomotor example input

Run `Scripts/migrate_locomotor_input.py` to install the native controller and walker hooks. The original GameMode still selects the imported controller and walker, and the walker retains its animation assets and movement component.

The Win64 Editor unity build and both Blueprint compiles passed. In `LocomotorLevel` PIE, the sandbox mapping context was installed at priority zero. Enhanced Input forward action moved the walker along the control heading, and both look actions changed yaw. Physical device sensitivity and remote-client restart are **Not run**.

### Look At POI controller

Run `Scripts/migrate_look_at_controller.py` to install the native interface setup and possession switch. The Blueprint events remain callable by the authored interface and forward directly to C++. The existing look-at and gesture animation controls remain authored.

The Win64 Editor unity build and controller Blueprint compile passed. In `LookAtPOI_Level` PIE, the controller initialized the widget and the 32-degree camera default. The original spawn event possessed the Mover pawn and reused it on a second call. The spectator event returned to the same spectator and cleared the spawned pawn reference and look-at flags. The smoke test called the existing events directly. Pointer interactions, selected-zone cleanup, and multiplayer are **Not run**.

### Look At POI zone selection

Run `Scripts/migrate_look_at_zones.py` to install native selection and gizmo lifetime. The existing click event and deselect event forward to C++. Hover presentation and the animation callbacks remain authored. C++ finds neighboring zones in the current world instead of relying on a startup snapshot.

The Win64 Editor unity build and zone Blueprint compile passed. In `LookAtPOI_Level` PIE, the original click event on Patrol broadcast its existing delegate once, created the target gizmo, and reached the controller's look-at callback. Selecting Loops cleared the previous zone and gizmo. The existing spectator event cleared selection and gizmos from all three zones. Physical mouse hit testing and rendered animation comparison are **Not run**.

### Look At POI pointer bindings

Run `Scripts/migrate_look_at_pointer.py` after the zone selection migration. Native code binds the authored hit components, updates hover and selection colors, and tracks the assigned pawn with the active gizmo. The existing tooltip layout and material assets remain authored.

The Win64 Editor unity build and zone Blueprint compile passed. In `LookAtPOI_Level` PIE, the component delegates were bound. Broadcasting their hover, leave, and click events applied white, gray, and green to both materials respectively. After moving the assigned pawn, the gizmo matched the authored offset. Physical mouse hit testing and rendered comparison are **Not run**.

### Look At POI target controls

Run `Scripts/migrate_look_at_gizmo.py` to install native target input and hover handling. The target mesh, hitbox, colors and tooltip remain authored.

The Win64 Editor unity build and gizmo Blueprint compile passed. In `LookAtPOI_Level` PIE, selecting a zone spawned the native target. Scripted pointer samples verified that Shift could start control only on hover, that control persisted after leaving the hitbox until Shift was released, and that a 10 by 7 pixel movement changed yaw by −10° and roll by 7°. The wheel increased scale during control and left it unchanged after release. Physical hardware input and rendered tooltip comparison are **Not run**.

### Mover paired interaction input

Run `Scripts/migrate_mover_interaction_input.py` to bind both actions in the ragdoll pawn and forward existing scripted requests. C++ checks the aim state and eligibility before choosing the demo callback or dispatching the current candidates to the authored paired animation graph.

The Win64 Editor unity build and the ragdoll pawn compile passed. In separate `RagdollLevel` PIE runs, injected `IA_Interact` and `IA_Takedown` actions assigned Attacker and Victim roles to two pawns and selected the corresponding authored shove and takedown assets. Aim-only demo callbacks, montage interruption, and multiplayer are **Not run**.

### Mover paired interaction collision

Run `Scripts/migrate_mover_paired_collision.py` to install scoped collision overrides and mesh tick ordering. The original montage callbacks release the native pair on blend-out or interruption. Cleanup preserves overrides that predated the interaction.

The Win64 Editor unity build and pawn Blueprint compile passed. In `RagdollLevel` PIE, a shove installed reciprocal capsule ignores. Interrupting the victim montage removed both ignores and retained the initiator's pre-existing ignore for a third actor. Dedicated tick-order instrumentation, natural montage completion, and multiplayer are **Not run** for this batch.

### Mover ragdoll rolling and settled recovery

Run `Scripts/migrate_ragdoll_behavior.py` to install the native ragdoll update, slope-assisted rolling, and delayed NPC recovery. The impact prediction, animation pose handoff, and get-up playback retain their authored paths.

The Win64 Editor unity build and pawn Blueprint compile passed. In `RagdollLevel` PIE, injected forward input produced a unit roll force while the player remained in Ragdoll. An unpossessed pawn returned to Walking about 3 seconds after the first observed Ragdoll frame, with speed below 10 cm/s. Steep-slope comparisons, exact spine torque instrumentation, and multiplayer are **Not run**.

### Mover manual ragdoll and contact dispatch

Run `Scripts/migrate_ragdoll_trigger.py` to bind the manual action and forward default-profile and contact logic. Manual and obstacle entries share the original animation-facing event. C++ evaluates contact bone speed and normal impulse and forwards the resulting gain to the existing Foley event.

The Win64 Editor unity build and pawn Blueprint compile passed. In `RagdollLevel` PIE, the input action entered Ragdoll. Changing the configured default switched the stored profile without replacing the active ragdoll profile. Recovery returned to Walking and applied the new default. Audible contacts and multiplayer are **Not run**.

### Windows C++ handoff

The Explorer-registered Unreal Version Selector command generated project files from `GameAnimSampleCpp.uproject`. The generated C++ project listed the Game and Editor targets and the new runtime sources. `Scripts/GenerateVisualStudioProjects.ps1 -VerifyOnly` checked the references in all four solutions.

Visual Studio 2026 loaded the C++ project from both game solution formats and displayed the AutomationUtils and BuildGraph projects in both automation solution formats. The Win64 Development Game target also built with unity enabled. Packaging and building every .NET automation project from the IDE are **Not run**.

### Clean reference installation

The Win64 Editor target built in an independent reference workspace. The ordered migration command then applied all 43 stages to a fresh copy of the acquired sample using binaries from that build. It completed in two Editor processes and restarted after the Mover parent recompile dirtied a dependent package. The five map packages and the primary CMC and Mover Animation Blueprints matched the original sample by SHA-256 after migration. Runtime replay in this second reference workspace is **Not run**.

A later clean installation built the Win64 Development Editor target from source in a short-path workspace and applied all 59 stages. The runner resumed in new Editor processes after stages 24 and 48. All five maps and both primary character Animation Blueprints again matched the acquired reference packages by SHA-256 after migration. Gameplay replay in this isolated copy is **Not run**.

### Sandbox pawn possession setup

Run `Scripts/migrate_pawn_setup.py` to forward the CMC and Mover setup entry points. The existing possession callbacks still choose when to install the local input mapping and activate the authored camera. C++ also tracks CMC simulated-proxy ground transitions and forwards velocity to the original jump and landing presentation.

The Win64 Editor unity build, both pawn Blueprint compiles, and Visual Studio project generation passed. In `DefaultLevel` PIE, the local Mover had the sandbox mapping and switched between the legacy camera and the Gameplay Camera. Possession of the CMC pawn activated its Gameplay Camera. Calling the existing simulated update across Falling and Walking preserved the supplied landing velocity and set `JustLanded`. The simulated proxy test supplied those states directly. Remote-client playback and a fresh 44-stage installation are **Not run**.

### Mover startup and frame update

Run `Scripts/migrate_mover_runtime.py` to connect the primary Mover lifecycle events to native code. Startup registers the pre-simulation, finalization, and based-movement callbacks and captures the spawn transform. The native frame update preserves the original order and calls the sliding audio and debug presentation hooks.

The Win64 Editor unity build and primary pawn Blueprint compile passed. In `RagdollLevel` PIE, the inherited ragdoll pawn had all three callbacks bound and stored the expected spawn and floor snapshots. Injected crouch input reached simulation. The cached mode tracked Ragdoll and the observed Falling exit, and crouch had cleared at that exit. Moving-base rotation and rendered audio or debug presentation are **Not run**.

### Ragdoll pawn startup and frame update

Run `Scripts/migrate_ragdoll_runtime.py` after the primary Mover runtime migration. The native setup creates the authored Physics Control modifiers, binds the post-animation callback, and establishes its dependencies. The native tick inherits the Mover update, then refreshes the debug state and runs ragdoll and interaction checks. Curve-driven strengths and presentation still use the authored post-animation graph.

The Win64 Editor unity build and ragdoll pawn Blueprint compile passed. In `RagdollLevel` PIE, setup created 23 body modifiers and bound the post-animation event. The debug CVar reached the runtime snapshot, the cached mode matched Ragdoll, and candidate selection cleared during ragdoll. Recovery restored Walking and PhysicalAnimation. Frame-level tick instrumentation and multiplayer are **Not run**.

### CMC startup and pre-movement tick

Run `Scripts/migrate_cmc_runtime.py` to connect native startup and pre-movement processing. The startup preserves the mesh and movement tick prerequisites and subscribes to movement updates only for simulated proxies. The original Blueprint entry points remain callable.

The Win64 Editor unity build and pawn Blueprint compile passed. In `DefaultLevel` PIE, the local CMC pawn bound the pre-movement tick and left the simulated-proxy observer unbound. Injected movement and sprint input raised the configured speed cap from 500 to 700 cm/s. Actual remote-proxy binding and frame-level tick timing are **Not run**.

### Sandbox camera style input

Run `Scripts/migrate_camera_style_keys.py` to install native scroll and D-pad bindings on both sandbox pawn classes. The original style range, feedback text, sound reference, and camera assets remain in use.

The Win64 Editor build and both pawn Blueprint compiles passed. In `DefaultLevel` PIE, injected mouse-wheel and D-pad keys changed the Mover style in both directions and stopped at the range limits. The CMC pawn reached the upper limit from zero using the D-pad. Physical input devices, audible feedback, and rendered camera comparisons are **Not run**.

### Mover flight shortcuts

Run `Scripts/migrate_mover_flight_keys.py` to replace the original shared flight shortcut graph on the Mover pawn. The native handler keeps the debug Z binding in non-Shipping builds and binds the left stick click through the pawn input component. Both shortcuts request Flying or Falling through the existing Mover queue and preserve the original feedback.

The Win64 Editor build and the Mover pawn Blueprint compile passed. In `DefaultLevel` PIE, injected Z input changed Walking to Flying, and injected left stick input produced the flight toggle feedback. A frame-level observation of Falling, physical gamepad input, and a Shipping build are **Not run**.

### Mover crouch cleanup after mode transitions

Run `Scripts/migrate_mover_transition_crouch.py` to move the airborne and slide exit crouch branches into the native pawn. The authored finalize callback keeps its foley playback and audio fade. The native branches run at the same points in that callback.

The Win64 Editor build and the primary Mover pawn Blueprint compile passed. In `DefaultLevel` PIE, injected crouch input entered crouched Walking. A test elevation and queued Falling transition produced crouched Falling, then uncrouched Falling, and returned to uncrouched Walking. The sprint-specific slide exit and an audible comparison are **Not run**.

### Ragdoll demo target placement

Run `Scripts/migrate_ragdoll_demo_spawn.py` to route the aimed interact action's target placement and rotation to the native pawn. The ragdoll asset still supplies the target class. The trace uses the original object filter, and the spawn keeps its facing, collision policy and three-slot eviction rule.

The Win64 Editor build and ragdoll pawn Blueprint compile passed. In `RagdollLevel` PIE, four calls through the original entry point produced target counts of 1, 2, 3 and 3. The fourth call removed the first target. A scripted camera and a target capsule temporarily assigned to the trace's object channel exercised the rotation branch. It turned the target 90 degrees without increasing the target count. Unassisted controller input, a blocked spawn and multiplayer are **Not run** for this batch.

### Ragdoll demo aim and hit response

Run `Scripts/migrate_ragdoll_demo_aim.py` to connect the reference camera query, aim reticle and demo hit response to C++. The wrappers preserve existing callers. The demo trace keeps its object filter and applies the authored injury preset before the hit bone receives an impulse.

The Win64 Editor build and ragdoll pawn Blueprint compile passed. In `RagdollLevel` PIE, the existing aim function returned the evaluated camera child's origin and forward direction. A scripted camera lined up a trace on the pelvis of a standing NPC. The existing hit action put that pawn into Ragdoll with the Limp injury state and enabled skeletal physics. Rendered reticle comparison, impulse measurement, physical input and multiplayer are **Not run** for this batch.

### CMC landing state window

Run `Scripts/migrate_cmc_landing_state.py` to move the landing velocity and the retriggerable flag to C++. The landing event writes velocity before the existing foley call and opens the 0.3-second window after dispatch. The animation graph keeps reading the same fields.

The Win64 Editor build and CMC pawn Blueprint compile passed. In `DefaultLevel` PIE, two calls through the existing landing event updated the velocity immediately. The flag stayed active past the original expiration and cleared 0.333 seconds after the second call at the sampled tick rate. The second velocity remained available after the flag cleared. This check supplied both landing velocities directly. Real landing input, audible foley and remote proxy timing are **Not run** for this batch.

### Mover directional sectors

Run `Scripts/migrate_mover_direction_sector.py` to transfer the direction classification, rear-angle stability and sprint fallback into the native pre-simulation path. The existing function retains the authored rotation-offset Chooser and feeds its curve sample to the Mover command through the original interface.

The Win64 Editor build and primary Mover pawn Blueprint compile passed. In `DefaultLevel` PIE, 810 grounded snapshots replayed through the original graph and its replacement returned identical sectors, curve outputs and debug angles to the sampling tolerance of 0.0002 degrees. The replay varied previous direction, style, gait and input angle, including stationary inputs and rear-angle wraparound. It injected snapshots directly. Airborne, sliding and network gameplay replay are **Not run** for this batch.

### Ragdoll impact prediction

Run `Scripts/migrate_ragdoll_impact_prediction.py` to connect the existing impact query to the native pawn. Native ragdoll updates run the prediction immediately after sampling spine velocity. The animation interface retains its time and socket-local direction fields.

The Win64 Editor build and ragdoll pawn Blueprint compile passed. In `RagdollLevel` PIE, nine cases matched the original Blueprint's destination time and local angles within 0.001 degrees. The replay injected three spine velocities against a clear path, a fixture that ignored the PhysicsBody trace, and a trace-blocking fixture. The one-second horizon and early stops at 0.2 and 0.4 seconds matched. Rendered animation, debug drawing and networked physics are **Not run** for this batch.

### Look At gameplay buttons

Run `Scripts/migrate_look_at_gameplay_buttons.py` to bind Spawn, Despawn and Patrol Teleport in the native widget parent. The controller owns possession and the imported widget still defines the controls, layout and animation selection.

The Win64 Editor build and widget Blueprint compile passed. In `LookAtPOI_Level` PIE, broadcasting the authored buttons' click delegates switched from the existing spectator to the demo player and back, with the expected collapsed and visible states. The patrol reset moved a pawn displaced by 123.69 cm back to the authored position. Pointer hardware, rendered layout comparison and multiplayer are **Not run** for this batch.

### Traversal ledge construction

Run `Scripts/migrate_traversal_ledge_setup.py` to route spline collection and opposite pairing to the native block after parent construction. The spline points, normals and per-instance transforms stay authored.

The Win64 Editor build and traversal block Blueprint compile passed. In `DefaultLevel` PIE, all 28 placed traversal blocks exposed their four splines in reference order with the four reciprocal opposite mappings. A dynamically spawned block exposed the same wiring immediately after construction, and a repeated native rebuild produced the same list and map. Full traversal replay and editor-time actor reconstruction are **Not run** for this batch.

### Look At zone selection state

Run `Scripts/migrate_look_at_selection_state.py` to publish the zone type, assigned pawn, gizmo and animation instance in the native controller. The reference animation and widget callbacks still run after a successful selection.

The Win64 Editor build and Look At controller Blueprint compile passed. In `LookAtPOI_Level` PIE, selecting the Patrol and Gestures zones gave the controller and widget the matching animation instance, retained the current gizmo and enabled the authored look-at state. Selecting the spectator through the existing event stored the requested type and preserved the last valid pawn, gizmo and widget selection. Mouse input and rendered pose comparison are **Not run** for this batch.

### Smart Object approach and cancellation state

Run `Scripts/migrate_smart_object_approach.py` to move the approach distance and bearing into the native interaction component. The native cancellation state update runs after the authored completion request and before the existing playback gate closes.

The Win64 Editor build and interaction component Blueprint compile passed. In `NPCLevel` PIE, the original approach function returned the expected distance and bearing for an aligned slot, a rotated and scaled slot, and a coincident slot. A direct front-facing bench request set the active interaction flag, which the native cleanup cleared. Movement-input cancellation through the playback gate and completion on a remote peer are **Not run** for this batch.

### Smart Object motion-match approach task

`Scripts/migrate_smart_object_approach_task.py` reparents `STT_PlayAnimFromBestCost` to `USampleMotionMatchApproachTask` and replaces its three setup, navigation-distance, and velocity wrappers. The native task resolves the bound actor's animation and Mover components, registers the existing move and montage completion callbacks, and unbinds them on StateTree exit or reuse. Navigation distance follows the current path length when a path is available, with a direct actor-to-destination fallback. The task retains its montage choice, cost evaluation, and authored StateTree bindings.

On 2026-11-22, the Win64 Development Editor build passed and the saved approach task compiled during the migration. `GameAnimationSampleCpp.Migration.SmartObjectApproachTask` passed in `SmartObjectTargetedFinal.log`. The targeted test checks the imported class and StateTree fields, a 50 cm direct-distance fallback, a 70 cm navigation path, CMC velocity, and removal of a move-completion binding when the task is reused. Live NPC StateTree execution, montage completion, invalidated paths, and networked interaction parity are **Not run** for this batch.

An isolated second run of the approach-task installer exited successfully and reported `Smart Object approach task already migrated.` in `SmartObjectApproachRerun.log`.

### Native player Smart Object input dispatch

`Scripts/migrate_player_interaction_bindings.py` removes the imported component's two Enhanced Input event wrappers and installs the original playback-opening and cancellation chains behind native Blueprint events. The CMC and Mover pawns bind `IA_Interact` and `IA_Move` on their existing input components. The saved component enables the native opt-in only after its wrappers are replaced. The authoritative slot request and authored StateTree remain in their earlier migrated locations.

On 2026-11-22, the Win64 Development Editor and Game builds passed. `GameAnimationSampleCpp.Migration.SmartObjectInputBindings` passed in `SmartObjectTargetedFinal.log`. It checks both action bindings on the migrated component and confirms the native base remains opt-out. The two-test `GameAnimationSampleCpp.Migration.SmartObject` queue completed successfully. Real Enhanced Input events during NPCLevel PIE, cancellation timing, paired animation, and multiplayer interaction are **Not run** for this batch.

An isolated second run of the input installer exited successfully and reported `Native player interaction input already installed.` in `SmartObjectInputRerun.log`.

### Dedicated-server Smart Object authority and client synchronization

On 2026-11-22, interactive `NPCLevel` PIE ran with two connected clients and a separate dedicated-server world under one Editor process. The local PIE diagnostic log records three worlds, two server player controllers, and NPC movement positions replicated to both clients. The additional tests below used the existing native `TryStartPlayerInteraction` entry point on each PIE pawn and placed both clients and their server counterparts in front of the authored bench. The test harness temporarily maintained the same positions during the RPC exchange because standalone `SetActorLocation` was subsequently corrected by Mover network movement. The tests did not synthesize a physical `IA_Interact` key press.

Before the authority fix, the Smart Object server PIE diagnostic recorded both client-side requests returning false and allocating local StateTree components, while the two corresponding authority calls returned true and entered the interacting state. The client states stayed false after the server requests. The earlier single-player PIE accepted the bench-front request and rejected the rear approach.

The native interaction component now replicates its authoritative activity bit, sends owning-client requests to a reliable Server RPC, validates the target and available slot from the server pawn and server Smart Object subsystem, and acknowledges successful acceptance to the owning client. Cancellation input is forwarded to authority and still dispatches the authored Blueprint cancellation event locally. Rejected requests no longer allocate local StateTrees merely to run a geometry trace. The imported Blueprint keeps its authored multicast montage-playback function and StateTree animation decisions.

After the fix, the Smart Object RPC PIE diagnostic records two client requests reaching authority. Both server player components became active with one server StateTree each. Both owning clients became active through network synchronization and had no locally allocated StateTree. After four seconds the activity bits still matched, although the test position was subsequently corrected when the harness stopped holding it. The client animation instances returned no active montage at that final sample, so actual paired playback, montage completion and visual parity remain **Not verified** by this check.

Win64 Development Editor and Game builds succeeded. The local Smart Object authority test log reports two successful targeted migration tests, including assertions that the component replicates, the new Server/Client RPCs carry the required reliable flags, the activity bit is replicated, and `PlayMontage_Multi` remains a Blueprint multicast. Real Enhanced Input injection, client-observer playback, cancellation completion, slot contention, packet loss and dedicated-server full animation parity remain **Not run** unless separately recorded below.

On 2026-11-23, a local two-seat alignment PIE diagnostic ran the authored bench at `(430, -10, 0)` in `NPCLevel` with a dedicated server and two independent CMC clients. The two server Smart Object slots at `(430, -55, 0)` and `(430, 35, 0)` were claimed independently. Both owners, remote observers and the server played the authored bench entry and idle montages. During idle, the server and both client representations agreed within 0.01 cm horizontally and about 0 degrees in yaw. Each capsule rested about 19 cm behind its slot in XY, with the authored animation providing the seated pose. `TwoSeatAlignmentPIE.result.txt` initially reported **FAIL** because both slots remained claimed after cancellation. Repeating cancellation only on the server produced the same result. In the isolated `TwoSeatNoNpcPIE.result.txt` run, five ambient NPC pawns were removed before player interaction. Both player claims then released, progressing from `CLAIMED` to `FREE` within six seconds (`TEST_RESULT=PASS`). This isolates ambient NPC competition as the reason the original test cannot use slot occupancy alone as proof of a leaked player claim. The legitimate NPC seat reuse behavior is preserved.

An interactive two-client session showed both idle montages running with matching server, owner and observer transforms. Skeletal mesh contact against the bench surface remains **Not verified**.

The focused owning-client replay removed the ambient NPCs from the test world to isolate player claim release. Both clients submitted `IA_Interact` through Enhanced Input, then cancelled through `IA_Move`. The owning client sends one cancellation RPC when its authored loop gate accepts movement. In each of two consecutive cycles, both seats were free by the six-second sample and the activity flags and active montages cleared on the server, owners and observers. Both clients reused the same seats. This was single-process PIE with scripted input and supplied approach positions. Physical input, rendered skeletal parity, separate-process networking and packet loss are **Not run**.

### Server-authoritative level styles and multiplayer spawn clearance

`ASampleLevelVisuals` replicates `ReplicatedStyleIndex` from authority and calls the authored visual update on each receiving client. Both overlap-driven and simulated `ASampleLevelButton` presses execute only on the server. `ASampleMultiplayerSpawnGameMode` tests the authored spawn against player capsules, blocking geometry and pressure-plate trigger bounds before searching nearby walkable floor. The sandbox GameMode inherits this policy, and the Locomotor GameMode is reparented by `Scripts/migrate_multiplayer_spawn_game_mode.py`.

In interactive Editor PIE, the five maps (`DefaultLevel`, `NPCLevel`, `RagdollLevel`, `LookAtPOI_Level` and `LocomotorLevel`) were each loaded with **Play as Client**, five owning clients and one dedicated-server world. `AllLevelsFiveClientsAuthority.txt` records one owned pawn per client. Server and all five clients agreed on the initial style and on a server-triggered style change. A client-side attempt to call `SetLevelStyle` did not change the selected style on any peer. In `DefaultLevel` and `RagdollLevel`, the five server-side spawn positions stayed at least 102 cm from the center of the nearest color-changing button and 180 cm from each other. The Look At POI map uses its non-colliding spectator pawns and permits their shared spawn point. The checks exercised scripted server and client calls. Randomized collision stresses, manual input and separate-process networking are **Not run**.

The paired 2026-11-24 startup runs loaded the reference and candidate with one dedicated server and three clients in a single Editor process for each map. Both projects had three possessed server pawns, the same GameMode and server pawn classes, and 3/3 replicated pawn matches on every client for all five maps. Look At POI matched on the observed spawn positions and movement modes, including its shared spectator spawn. In DefaultLevel, NPCLevel, and RagdollLevel, the smallest sampled server pawn spacing was 0 cm in the reference and 180 cm in the candidate, consistent with the native clearance policy. LocomotorLevel changed from 68.2 cm to 180 cm. The project owner accepted retaining the native multiplayer spawn clearance. In each of DefaultLevel, NPCLevel, and RagdollLevel the reference snapshot contained one Walking and two Falling pawns, whereas all three candidate pawns were Walking. The two coincident reference positions in each map and the candidate floor-projected clearance offsets are consistent with a spawn-dependent grounding difference. Unreal Mover evaluates walkable floor contact when selecting its grounded or airborne movement mode. The snapshots do not establish the exact floor-query result or movement-mode transition for each pawn, so the causal explanation remains provisional. These captures cover startup possession and observed replication. Movement after input and network correction still require comparison.

A subsequent Win64 Development archive test ran each project separately as one listen server and two independent client processes on loopback. Both clients joined and loaded the expected world in each of the DefaultLevel, NPCLevel, and LookAtPOI_Level runs. All six hosts and all twelve clients exited normally. The test exercised connection and startup. Separate-process gameplay replication, movement prediction and network correction remain open.

In the subsequent `ThreeLevelsFiveClientsCmc.txt` diagnostic, `DefaultLevel` and `NPCLevel` were tested with `DDCvar.PawnClass 0`, while `RagdollLevel` used its authored Mover ragdoll pawn. Every one of the five owning clients spawned in `Walking`, and each client observed all five server pawns at matching positions in all three maps. There was no Flying state at spawn. Locomotor and Look At POI remain covered by the earlier all-level ownership and style test. This diagnostic did not inject physical movement input or measure correction after movement. Play settings and the pawn-class CVar were restored when it ended.

On 2026-11-23, `PressurePlatePhysicalPIE.txt` recorded a repeat test with one dedicated server and eight clients in each of `DefaultLevel` and `RagdollLevel`. All eight player pawns spawned outside the authored pressure-plate bounds. Moving a server-owned pawn into each of the Dark, Light and Colorful triggers caused the server's style selection to propagate to all eight clients. Every sampled world agreed on both `StyleIndex` and the applied decal color after the three physical trigger events. The test ended with `TEST_RESULT=PASS failures=[]` and restored the existing five-client Play as Client settings. This was a single-process, nine-world PIE test using scripted actor placement.

`PressurePlateTimingPIE.txt` additionally sampled the DefaultLevel presentation from 0.5 to 20 seconds after each trigger. During the first change, the server had selected Dark at 0.5 seconds while clients still displayed the previous style. All nine worlds agreed at 1.0 second and at every later sample through 20 seconds. The subsequent Light and Colorful changes agreed from the first 0.5-second sample. The observation establishes eventual replication under this test's normal Editor networking conditions. A strict subsecond convergence guarantee, packet-loss behavior, separate-process behavior and late-joining clients remain **Not run**.

On 2026-11-23, the native Win64 Development Editor build completed successfully and the Win64 Development Game target remained up to date. Both `GameAnimationSampleCpp.Migration.SmartObjectApproachTask` and `GameAnimationSampleCpp.Migration.SmartObjectInputBindings` completed with `Result={Success}` in a fresh headless `UnrealEditor-Cmd.exe` run. A separate `Automation RunTests GameAnimationSampleCpp.Migration` run completed all 16 migration tests with `Result={Success}` and no failed tests. `Scripts/GenerateVisualStudioProjects.ps1 -VerifyOnly` resolved all four existing Visual Studio solutions, including the Game and AutomationTool `.sln` and `.slnx` formats. This verification checked references and project-file contents without regenerating files or launching MSBuild.

### Traversal component startup

Run `Scripts/migrate_traversal_startup.py` to connect the component's BeginPlay to native startup. It caches the original character interface result and resolves the owning Character Mover Component when the asset selects Mover playback.

The Win64 Editor build and traversal component Blueprint compile passed. In `DefaultLevel` PIE, the possessed Mover pawn and a newly spawned CMC pawn both cached their own capsule, mesh and motion-warping components. An explicit refresh matched the complete interface snapshot for each pawn. The Mover reference was present only on the Mover pawn. Full traversal replay and multiplayer startup are **Not run** for this batch.

### Accepted traversal dispatch

Run `Scripts/migrate_traversal_dispatch.py` to route accepted traversal results through native storage and dispatch. The existing Chooser selects the montage. The native path stores its typed result before starting authored local playback and invokes the original server RPC with the stored result.

The Win64 Editor build and traversal component Blueprint compile passed. In `DefaultLevel` PIE, a supplied approach to `LevelBlock_Traversable_C_4` selected an authored hurdle montage. The component published the matching obstacle and montage and entered its active traversal state.

In two-player listen-server PIE, the owning Mover client initiated the hurdle using the same supplied approach. The authority copy received the same montage and obstacle through the existing RPC. Both copies entered Traversing, cleared their active flags and returned to Walking. The test positioned both copies before dispatch. CMC playback through this dispatch path and unassisted input are **Not run**.

### CMC traversal network recovery

When the authority's montage ended before the owning client's montage, CMC could first accept an in-flight Flying move and then send a Flying correction back after the grace period elapsed. The owner applies the authored exit mode at montage completion. The remote authority keeps accepting movement until it sees an exit from Flying, with a two-second timeout before the next recovery check applies the authored exit mode. The owning client, listen host and simulated copies keep their local 0.2-second correction grace period. Starting another montage cancels the previous recovery timer.

In two-player listen-server PIE in `DefaultLevel`, the owning CMC client started the authored vault from the supplied placement. Before the fix, the owner briefly reached Falling, then returned to Flying alongside the authority. With the fix, both copies played the same vault montage, reached Falling and settled into Walking at 3 FPS and 60 FPS with both correction flags cleared. At four game seconds the positions were 1.11 cm apart at 3 FPS and matched to the log's 0.001 cm precision at 60 FPS. The test used `DDCvar.PawnClass 0`, `t.MaxFPS 3` or `t.MaxFPS 60`, and the existing `TryTraversalAction` entry point.

The Win64 Development Editor built, and `GameAnimationSampleCpp.Migration.TraversalMovement` passed. It verifies that authority holds Flying and accepts the owner's move after finishing the local montage. The timeout fallback, packet loss, dedicated server and other vault approaches are **Not run**.

### Native traversal gameplay installation

`AC_TraversalLogic` retains its `TryTraversalAction`, `SetWarpTargets`, and `EventGraph` entry points. The source graph export confirmed that each inspected `GetArrayItem` warp lookup used the disconnected integer input `Dimension 1` with default `0`. The installer verified the source graph, compiled and saved the native forwarding calls, and preserved the parent class and all seven checked function flags, including the server and multicast flags. The installed package and recovery backup kept their SHA-256 values across repeat installation and a fresh Editor process. The `SandboxCharacter_Mover_Ragdoll` movement-mode transition wrapper also passed its compiled-call and package checks after reopening.

On UE 5.9.0 build 60738873, Win64 Development Game and Editor targets passed. `TraversalCharacterState`, `TraversalLedges`, `TraversalMovement`, and `TraversalWarpGeometry` passed. These checks cover native movement-state and geometry behavior and the saved asset wiring. Network delivery, packet emulation, and correction after traversal input remain pending. The earlier playback comparisons use a prior migration state.

### Pawn traversal input queries

`Scripts/migrate_traversal_input_wrappers.py` routes both sandbox pawns' existing `GetTraversalCheckInputs` functions to the builders used by native jump input. Callers still receive `S_TraversalCheckInputs` with the original field names and types. The CMC query keeps its grounded speed range and Falling and Flying sweep. The Mover query uses finalized movement inputs for airborne direction and preserves the imported defaults in modes outside its query.

An unattended Editor comparison called both Blueprint functions before and after conversion. All six sweep fields matched in 13 cases, comprising CMC Walking, Falling and Flying at four velocities and the Mover pawn at rest. Both Blueprints compiled. The installer ran a second time and reported the queries already installed. Both Win64 Development targets built. The `GameAnimationSampleCpp.Migration.Traversal` filter passed its ledge and movement tests. The five map packages and the primary CMC and Mover Animation Blueprints matched the original sample by SHA-256. Moving and airborne Mover comparisons, physical input and rendered traversal are **Not run** for this change.

### Pawn traversal character state

`Scripts/migrate_traversal_character_state.py` routes both existing `Get_PropertiesForTraversal` functions to C++. The interface still returns `S_CharacterPropertiesForTraversal` at the original call sites. Component references remain owned by their pawn. CMC reads its movement mode, current gait and horizontal speed. Mover uses its finalized gait, mode map and horizontal velocity.

`GameAnimationSampleCpp.Migration.TraversalCharacterState` passed on the original graph bodies and on the native wrappers. It verified component identity and all three movement fields for seven CMC modes at three gait and velocity settings, and two Mover modes at three gait settings. The Mover cases supplied different pre-simulation and finalized gait values. Separate assertions exercised all six fields of the migrated Mover sweep in Walking and Falling with a nonzero horizontal velocity and finalized airborne input. The enclosing `GameAnimationSampleCpp.Migration.Traversal` filter passed all three tests. Both Win64 Development targets built. Both pawn Blueprints compiled, and rerunning both traversal wrapper installers reported the native calls already installed. Rendered traversal and physical input are **Not run** for this change.

### Pawn camera character state

`Scripts/migrate_camera_character_state.py` routes both `Get_PropertiesForCamera` functions to `USampleCameraStateLibrary`. The existing interface still returns `S_CharacterPropertiesForCamera` to the camera assets. CMC uses its current camera style, stance and gait, and resolves aim before strafe. Its movement mode field keeps the struct default. Mover reads facing and gait from its finalized input, lets twin-stick override facing, and maps its current movement mode through the pawn's existing mode map. Mover keeps the authored camera style and returns the struct defaults when the named movement component is missing.

`GameAnimationSampleCpp.Migration.CameraCharacterState` passed on both the original graph bodies and the native wrappers. It checked all five fields in 17 cases covering CMC aim and strafe priority, style changes and crouching, and Mover Walking, Falling, differing pre-simulation and finalized inputs, twin-stick and the missing movement component. The full migration filter passed all 18 tests. Both Win64 Development targets built, both pawns compiled, and a second camera installer run found both native calls already installed. All five maps and the two primary Animation Blueprints matched the original packages by SHA-256. The registered Windows project generation command and the four Visual Studio reference checks passed. Mover crouch, physical camera input and rendered camera comparison are **Not run** for this change.

### Sandbox still camera

`Scripts/migrate_sandbox_fixtures.py` gives `StillCam` a native parent that updates its tracking transform. The asset retains its camera component and existing fields. The native tick aims before following and records the target's current location even with follow disabled, so enabling follow uses the next movement delta. A missing target leaves the camera and its last location alone.

The Win64 Development Editor built, the fixture migration compiled the asset, and its second run saved no package. `GameAnimationSampleCpp.Migration.StillCamera` passed with the migrated generated class. It covered look-only, simultaneous look and follow, and loss of the target. Switching through the sandbox widget and checking the rendered camera view are **Not run**.

### Mover movement debug overlay

`Scripts/migrate_mover_debug_draw.py` replaces the sandbox Mover's `DebugDraws` and `Get_MovementDirectionThresholds` bodies with native calls. The regular pawn update submits debug shapes after updating analog deflection. The overlay uses the local-control and `DDCvar.DrawCharacterDebugShapes` gates, the capsule base, root facing, target orientation, rotation offset and current movement intent. It submits the same shapes to the scene and Visual Logger. The signed thresholds remain available through the existing struct interface. The native rotation sector calculation and debug pass share the style and sector limits.

`GameAnimationSampleCpp.Migration.MoverDebugDraw` passed with the original graphs and the migrated pawn. It checked the control and CVar gates and all four signed fields for 54 combinations of rotation mode, style and sector. An ordered line-buffer comparison matched all eight geometry samples, including segment endpoints rounded to 0.001 cm, colors, thickness and depth priority. The fixtures recorded 63 segments at idle, 116 with movement intent and 223 for each of six strafe sectors. The full migration filter passed all 19 tests. The asset compiled and a second migration run made no edits. The five maps and two primary Animation Blueprints matched the source by SHA-256. Win64 Development Game and Editor builds, Doxygen and Visual Studio solution generation passed. Interactive rendering and Visual Logger recording capture are **Not run**.

### Shared animation debug drawing

`Scripts/migrate_shared_debug_drawing.py` forwards seven functions in `BFL_HelpfulFunctions` to `USampleDebugDrawing`. Both primary Animation Blueprints retain their existing call sites and choose which debug data to submit. The native functions draw direction markers, heading thresholds, boolean states, string or object lists, and multiline histories. The chart keeps the imported `S_DebugGraphLineProperties` type, series and legend order, and authored ranges. String history still inserts at the front and removes only the value at the requested index.

`GameAnimationSampleCpp.Migration.SharedDebugDrawing` passed through the original library and through the installed native forwards. Six scene draw buffers matched by ordered line count, endpoints rounded to 0.001 cm, color, thickness and depth priority. The same fixture verified string history with an oversized input list. The full migration filter passed all 20 tests. The shared library compiled and the second installer run reported that all forwards were already installed. The five maps and two primary Animation Blueprints matched the source by SHA-256. Win64 Development Game and Editor builds, Doxygen and Visual Studio solution generation passed. Interactive rendering, the optional engine debug label on the radial arrow, and Visual Logger recording capture are **Not run**.

### Animation diagnostic text formatting

`Scripts/migrate_mover_animation_diagnostics.py` replaced six CMC and seven Mover text formatters with native calls at their existing consumers. Both Animation Blueprints compiled and saved. The original diagnostics-stage backups matched the pre-install packages. A second installation preserved both package hashes, both backups, and the exported diagnostic graphs. The graph audit compared existing node connections and the native replacements through the respective CMC and Mover entry points.

The Standalone PIE comparison recorded 24 idle cases on each side, covering the diagnostic CVar combinations in both locomotion setups. The pawn classes, Animation Blueprint classes, local-control status, positions, velocities, and CVar settings matched. Two CMC samples differed in startup animation state and transition history. All eight Mover samples differed in one or both recorded motion-matching search costs. Some paired samples ran at different elapsed times. A separate Editor test passed all 63 equal-input comparisons of the reflected native formatters against the original Kismet string operations. This verifies the formatting of identical inputs. Moving input, the source of the differing animation samples, matched rendered output, and Visual Logger parity remain open.

A later native Standalone run exercised movement and look input in all four CMC and Mover setup combinations. The engine dispatched `W` and the mapped gamepad look action, and all four pawn profiles moved, rotated the camera, and returned to zero speed after release. Setup 1 in both Animation Blueprints returned a null `CurrentSelectedAnim` throughout the captured interval. The new Mover setup 0 sample reported `0.13964758813381195` for `Search Cost` at the ready, movement, and final milestones, matching the earlier pre-formatting sample. The reason for the earlier zero sample and the comparison with the acquired project remain open.

The corresponding Standalone run in the reference fixture used the same four profile schedules. The fixture matched the acquired sample on the five maps and both primary Animation Blueprint package hashes. All four reference profiles responded to movement and look input and returned to zero speed. Both projects observed a null `CurrentSelectedAnim` throughout setup 1. Displacement measured before releasing `W` differed by less than `0.80 cm` for each CMC profile and by about `13.49 cm` and `3.87 cm` for Mover setups 0 and 1, respectively. The sampled camera response differed by at most `0.021°`.

A follow-up recorded Mover's pre- and post-simulation ticks. Both projects used `MoverNetworkPredictionLiaisonComponent`. The reference used Independent ticking and the native project used Fixed ticking. Every native simulation step in that run was 16 ms. A native launch with an Independent override reproduced the reference's usual 16–17 ms steps. The maintained `Config/DefaultNetworkPrediction.ini` then applied Independent ticking and Interpolated proxy LOD on a normal native launch. Across three repetitions per setup, the largest X-displacement difference at the four matched simulation times was `0.386 cm`. Setup 0's final Search Cost matched exactly. Setup 1's paired final costs still differed by up to `0.001913`. Separate-process network prediction and rendered pose parity for this configuration are **Not run**.

### Win64 release verification baseline

The consolidated follow-up rebuilt both Win64 Development targets and ran the migration and SampleLocomotion Automation filters through the Editor. All 25 tests passed. The three added cases cover diagnostic row formatting and a null animation context, controller input-display math, and clearing the native and authored player interaction flags. The interaction test exercises direct cleanup on authority. The movement input and network release paths require their separate gameplay checks.

After the Look At gizmo-target upgrade, the rebuilt Win64 Development Editor ran `Automation RunTests GameAnimationSampleCpp.Migration` and `Automation RunTests SampleLocomotion`. All 23 migration tests and both locomotion tests passed. The Editor's original background CPU setting and PIE preferences were restored. Garbage collection removed only transient test maps. The final package fingerprints matched the post-install checks.

On 2026-11-24, the native baseline revision was `32de464e530c17204df87643df04864eddd4f989`. The acquired sample descriptor reported `EpicSampleNameHash` `1673045636` and had SHA-256 `db56d0061e71847e09aa8cd7ad3e729309c33582f915c2f6c2bfaa28b21dda9a`. Both Content trees contained 3,898 files at the same relative paths. SHA-256 matched for 3,845 files. The 53 pre-existing differences were all `.uasset` packages. The five maps, `SandboxCharacter_CMC_ABP`, `SandboxCharacter_Mover_ABP`, and all three Look At POI movies matched. Separate working copies matched their respective baselines for all 3,898 Content files.

Unreal Engine 5.9.0 build 60738873 and Visual Studio 2026 18.11.12306.390 were installed. The Win64 Development Editor and Game builds for the reference copy succeeded with MSVC 14.50.35739 and Windows SDK 10.0.26100.0. The commands used the engine's `Build.bat` with targets `GameAnimationSampleEditor` and `GameAnimationSample`, followed by `Win64 Development -Project=<reference-copy.uproject> -WaitMutex -NoHotReloadFromIDE`. Each build exited with code zero and wrote its target receipt.

The first reference `BuildCookRun` attempt stopped at cook startup with a missing Editor target receipt. UAT treated the content project as code-based because AnimationWarping was enabled and generated temporary targets. The subsequent Win64 builds generated the receipts in the working copy. This baseline entry establishes fixture and build evidence. Successful cook, stage, package and packaged play are recorded separately when verified.

The reference fixture used for the next attempt was prepared inside the project context. All 3,898 Content files matched the acquired sample again. Both Win64 Development targets built successfully from the relocated descriptor and wrote new receipts for that location. The relocated reference then completed Win64 Development cook, stage, package, and archive with AutomationTool exit code 0. The five maps were explicitly passed with `-MapsToCook`, and the cooker reported 3,626 packages complete. The archive contains the game executable and IoStore containers. An initial map check expected loose files and failed. `UnrealPak.exe -ListContainer=<project.utoc> -CSV=<map-inventory.csv>` then exited with code 0 and identified one nonempty export bundle for each of the five `.umap` files in the archived container. The three Look At POI movies are staged as loose files under `Content/Movies/LookAtPOI`. Each matches both the acquired sample and the native source copy by SHA-256. All three corresponding File Media Source packages have nonempty exports in the archived IoStore container.

The archived reference Win64 Development executable opened all five maps in separate processes with `-windowed -unattended -NoSplash`. Each run's log confirmed that the requested world began play, and each process exited with code 0 after normal engine shutdown. The Direct3D SM6 window for `DefaultLevel` rendered the player character in the default level. Its initial automated summary missed the readiness lines because the log parser rejected space-padded frame numbers. The raw log and process receipt confirm the launch and exit. The corrected parser verified `NPCLevel`, `RagdollLevel`, `LookAtPOI_Level`, and `LocomotorLevel`.

The initial native Win64 Development cook processed 3,603 packages and completed stage, package, and archive with AutomationTool exit code 0. The stage and archive container hashes matched. Each included nonempty IoStore export bundles for all five maps and all three Look At File Media Source assets. The three loose MP4 files matched the local source movies by SHA-256. The first stage launch of `DefaultLevel` brought up the requested world, but the test force-closed it when its 10-second shutdown timeout expired. With a 60-second timeout, all five maps in both the native stage and archive brought up the requested world and exited normally with code 0. All five reference archive smoke cases also passed. Interactive archive results are recorded under the corresponding gameplay sections.

Physical testing of the initial native archive in `DefaultLevel` exposed missing runtime-loaded Enhanced Input packages, including `IMC_Sandbox`, `IA_Move` and `IA_Look`. The input directory is now explicitly included in the cook through `Config/DefaultGame.ini`. A fresh Win64 Development cook, stage and archive completed with AutomationTool exit code 0, and the verified executable was reused because its build inputs and products matched. Both stage and archive contained all 19 required input assets with nonempty IoStore exports, and their container hashes matched. On November 27, 2026, the project owner confirmed W movement, stopping after releasing W, and mouse look in the reference and repaired native archives. The repaired runtime initialized Enhanced Input without corresponding missing-package warnings and shut down normally after the window close request. The numeric process exit code was unavailable for this interactive session. In a subsequent separate smoke run, all five maps in the repaired archive reached begin-play with the normal renderer and exited gracefully with code 0. Physical gamepad input, other physical input actions, and media and networking rechecks for the repaired archive are **Not run**.

The integrated traversal archive completed a separate Win64 Development cook, stage and archive with exit code 0. Stage and archive container hashes matched. Each contained all five maps, 19 input assets, three File Media Sources and 12 checked migrated runtime packages. The three loose movies matched the source by SHA-256. Both executables matched the verified Game binary. `GameAnimationWidget` and its `WB_SimpleController` overlay were absent from both packaged inventories, consistent with their Editor utility caller and the reference archive. Each of the five maps reached begin-play and exited with code 0 in a separate rendered run from the archive. Physical input and network gameplay on this archive are **Not run**.

File Explorer's **Generate Visual Studio project files** action completed for the native descriptor. The generator processed both formats for the game and AutomationTool solutions. The AutomationTool `.sln` changed, and the other three outputs matched their previous hashes. Visual Studio 2026 opened all four paths in fresh build processes through `devenv.com /Build`. Both Win64 Development targets passed from both game solutions. UBT reported that the targets were up to date. Solution-wide automatic NuGet restore failed to load AutomationTool project details. After an explicit offline restore using project-local output paths, the selected AutomationTool build passed from each format and produced a new DLL. Interactive Solution Explorer verification is **Not run**.

Doxygen 1.18.0 generated the baseline HTML successfully using the repository's `Doxyfile` with the output path redirected. `Scripts/GenerateVisualStudioProjects.ps1 -VerifyOnly` resolved the four existing game and AutomationTool solutions, including both formats and matching project references. Explorer regeneration and opening freshly generated solutions were **Not run** in this baseline check.

### Look At POI Widget startup baseline

An Editor PIE session on UE 5.9.0 build 60738873 loaded `LookAtPOI_Level` before the About panel migration was installed. In Play as Client with three clients and one dedicated-server world, each server controller had no local `Level Widget`. Each client had one local `PC_LookAtPOI` controller whose `Level Widget` referenced its own `WB_LookAtPOI__GUI` instance. The check enumerated the PIE worlds and inspected the controller references. It establishes the multiplayer startup condition for the Widget comparison. Standalone initialization, About button interaction, page changes and decoded video playback are **Not run** in this startup check.

A separate interaction capture in the same pre-install PIE session broadcast the actual About and Close button delegates on one client and invoked the existing zone-selection entry point for each placed zone. The panel started visible, the Close button was collapsed and the switcher was on page 0. Repeated About clicks alternated the panel between collapsed and visible. Close left the panel collapsed. The gesture list remained Agree, Disagree, Come On In, with Come On In selected.

The Patrol, Loops and Gestures selections produced pages 3, 1 and 2 respectively. Each title and media source matched its authored graph literal, and each media player URL pointed to the selected movie. The immediate samples had no playing flag. Decoded playback, physical mouse input and post-install parity are **Not run** for this capture.

Standalone PIE in the same pre-install Editor started with the About panel collapsed, the Close button visible and the switcher on page 1. The same button and zone sequence produced three alternating About toggles, a collapsed panel after Close, and the same three titles, indexes and media sources as the client session. This preserves the baseline distinction between Standalone and network initialization for the candidate comparison.

The candidate `GameAnimationSampleCppEditor` Win64 Development target built successfully. The maintained `migrate_look_at_about_panel.py` installer compiled and saved the widget with its three native page calls. A second run identified the installed graph and made no package changes. The original backup and the twice-checked candidate package retained their respective SHA-256 hashes. The original and installed widget matched across 21 checks each in Standalone PIE and a Play as Client session with one server and three clients in a single process. The checks covered startup, the About and Close button delegates, the three zone selections, gesture options, and the selected media references. In the network session the interaction sequence ran on the first local client. Physical input and decoded playback remain pending.

Standalone PIE runs in both projects verified decoded playback for Patrol, Loops, and Gestures. The placed zones selected byte-identical movie files, and the existing About delegate exposed the active MediaTexture. The selected video tracks advanced while the panel was visible. Two captures per page showed source-matching content and movement in the video region. Each Close delegate hid the panel and paused WmfMedia. The playback positions stayed near the close point throughout each observation window. All six cases failed the return-to-start criterion even though the player reported seek support. The cause of the rewind behavior remains open. The captures were taken at different playback positions.

Both Win64 Development archives also ran `LookAtPOI_Level` in separate interactive sessions. Each initial view showed the instructions with the world labels outside the frame. The Development console placed the spectator at a diagnostic viewpoint before selection. Actual clicks on the world labels, About buttons, and Close buttons exercised all three zones in both archives. Two captures for each open panel showed changes inside the video surface, and Close dismissed the panel while preserving the selected zone. Both sessions shut down normally. The media-clock observations above come from PIE. Network video presentation is **Not run**.

A read-only graph export also followed the controller's `Zone Was Selected` event. After the native capture helper accepts a zone, the controller enables Look At, calls its authored animation-target function, and attempts the cast to the Look At Animation Blueprint. A successful cast enables the animation instance's Look At logic and calls the widget's `ZoneSelected` event. This order defines the remaining controller routing boundary. The authored target function and Animation Blueprint are retained.

The zone-routing installer compiled and saved `PC_LookAtPOI`. Of 50 fingerprinted Look At packages, only the controller package changed. Its backup matched the pre-install SHA-256, and a second installation left all 50 hashes unchanged. Normalized snapshots matched at 21 points in Standalone PIE and 13 points in Play as Client. The comparison exercised repeated selection through the placed Patrol, Loops, and Gestures zones. Standalone also exercised invalid pawn and animation instances. An explicit authority-side selection with no gizmo or local widget enabled the animation instance's Look At logic and set its target to `(0, 0, 0)` in both versions. Each of those network runs still logged six `Gizmo Obj` null reads in the authored target function.

The gizmo-target update compiled and saved the already routed controller. Only the controller changed in the package fingerprint set. The upgrade and original backups were preserved, and repeat installation left every sampled package hash unchanged. In the matched Play as Client replay, each local zone selection produced the same page and activation state as the authored route. The nonlocal authority controller had no gizmo or local widget. Its animation target remained `(0, 0, 0)` and Look At remained enabled after the selection and a following tick. The bounded replay log contained zero Blueprint runtime errors. A separate Standalone run passed 13 target checks covering rotated gizmos, positive, negative and zero X scale, the tick update, local null and destroyed-gizmo fallback, and the player head-socket branch.

### Look At POI level diagnostic baseline

The authored Level Blueprint enables two diagnostics at BeginPlay. It executes `a.AnimNode.ControlRig.Debug 1` followed by `ControlRig.EnableDrawInterfaceInGame 1`. The Destroyed event executes the same pair in the same order with value `0`. Each command obtains player controller 0 from the executing world. The Tick entry has no outgoing link.

The pre-migration log recorded five ordered enable pairs across one four-world Play as Client session and one Standalone session. Normal EndPIE produced zero disable pairs in those runs.

In a separate single-player Standalone PIE comparison, the original map and the installed native map both executed the two enable commands in order at BeginPlay. Explicitly destroying the persistent level-script actor executed the two disable commands in the same order. Both console variables read `1` before destruction and `0` afterward, and the actor was absent when queried again. The installer compiled and saved the forwarding graph, preserved the original map in its backup, and reported no map changes on a second run. The installed map and backup matched their recorded hashes after the comparison. Separate candidate runs recorded one ordered enable pair in Standalone and four in Play as Client. Ordinary EndPIE produced zero disable pairs in both modes, matching the baseline.
