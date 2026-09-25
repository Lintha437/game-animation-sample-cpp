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

In two-player listen-server PIE on `DefaultLevel`, an owning CMC client initiated the vault. At an uncapped frame rate, the client and authority finished in Walking with both flags cleared and positions within 0.01 cm. At `t.MaxFPS 3`, both remained Flying four game seconds after the start despite clearing the flags. The saved pre-mode-migration Blueprint reproduced the failure under the same frame cap and also landed in the uncapped control. Movement recovery under long frames still needs a network-path fix.

### Traversal replay recipients

The imported server and multicast declarations keep their original network flags and traversal payload. The multicast calls `USampleTraversalComponent::ApplyTraversalReplay`. The authority for a remote pawn and simulated copies install the typed result and invoke the authored playback event. The locally controlled pawn keeps its predicted playback. Receipt of the result during an active CMC traversal restores Flying if an earlier movement correction interrupted that mode.

Build the Editor target, close the destination Editor, and apply the local replay migration.

```powershell
$script = (Resolve-Path Scripts/migrate_traversal_replay.py).Path
& $editor $project -run=PythonScript "-script=$script" -Unattended -NullRHI
& $editor $project -Unattended -NullRHI '-ExecCmds=Automation RunTests GameAnimationSampleCpp.Migration' '-TestExit=Automation Test Queue Empty'
```

The local package is backed up under `Saved/BlueprintMigration/TraversalReplay/`. Both Win64 Development targets built and all eleven migration tests passed. The tests exercise local ownership gating and the original RPC directions. In two-player listen-server PIE at 60 FPS, the owning client and authority each started the selected vault exactly once. A host-initiated run likewise started once on the host and once on the observer. Both runs reached the lower platform and returned to Walking with the flags cleared, and the two copies in each run settled within 0.01 cm. At 3 FPS, the owning client and authority still remained Flying after four game seconds even though both flags cleared and each montage started once. Movement recovery under that condition remains open.

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

See the [Epic sample documentation](https://dev.epicgames.com/documentation/unreal-engine/game-animation-sample-project-in-unreal-engine) for the sample's public controls and architecture.
