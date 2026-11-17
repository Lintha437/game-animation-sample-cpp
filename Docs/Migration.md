# Game Animation Sample C++ migration guide

Use Unreal Engine 5.9 and a C++ project built with the associated toolchain. Start with the portable plugin if your project already owns possession, input, and animation. Use the licensed reference sandbox as a comparison level if you need to carry over the sample's authored animation stack.

| Source | Destination responsibility |
| --- | --- |
| `SampleLocomotion` runtime plugin | Install the plugin and integrate its CMC pawn, animation snapshot, or traversal geometry probe independently. |
| Project runtime module `GameAnimationSampleCpp` | Review the sample-specific character, tick, notify, and travel adapters before porting any of them. Their existing integrations use imported sample package paths and interfaces. |
| Locally imported Epic packages | Acquire and migrate the licensed assets separately. Keep the authored animation interfaces and verify each asset's dependencies in the destination. |

## Reproduce the native sample in a clean workspace

1. Acquire the Game Animation Sample through Epic Games Launcher. Keep that project available as the reference and use a short, independent Windows path for the destination.
2. Follow [local reference workspace setup](../README.md#local-reference-workspace) to copy the acquired content and local configuration, build the Win64 Development Editor, and run the ordered asset migration. Use the destination descriptor for every command. Preserve the reference project's package paths so the native adapters can find their authored fields and classes.
3. Read the migration log through its final stage. When a stage fails, inspect its per-process log and compare the affected package to its backup under `Saved/BlueprintMigration/`. Close the destination Editor before restoring a package or rerunning a stage.
4. Regenerate the Visual Studio projects from the destination `.uproject` or with the [project generation script](../README.md#develop-in-visual-studio). Verify both game and both AutomationTool solution formats. Build Development Editor for PIE and Development for the standalone Win64 Game target.
5. Open `DefaultLevel`, `LocomotorLevel`, `LookAtPOI_Level`, `NPCLevel`, and `RagdollLevel`. Use the [reference behavior inventory](ReferenceBehavior.md) to choose matching inputs, starting state, camera, movement system, and network role. Record both the completed action and its cleanup, including interruption.

## Compare a Win64 release candidate

Build the candidate in your existing native checkout. If running the acquired sample would write generated files, execute it from a local reference copy. Record the candidate revision, sample descriptor SHA-256, installed engine build, MSVC toolchain, and Windows SDK. Start both projects with matching map, input bindings, simulation settings, and player roles. Reuse each running Editor between compatible tests and return its Play settings and console variables to their original values when finished.

| Map | Behavior to compare |
| --- | --- |
| `DefaultLevel` | Test the CMC and Mover pawns independently. Compare digital and analog input, gait, camera style and aiming, traversal success and rejection, and the movement state after landing. Repeat a movement or traversal case with authority, owner and observer recorded. |
| `NPCLevel` | Compare NPC startup and patrol, player and NPC Smart Object eligibility, claim contention, interruption, and slot reuse. Observe state on the owning client and authority when testing with clients. |
| `RagdollLevel` | Compare ragdoll entry, contact and recovery at matching positions and velocities. Record movement and collision state on return to locomotion. |
| `LookAtPOI_Level` | Start in the authored mode. Use the interface and world selection controls to compare possession, selection and deselection, the About panel, the three page changes, and media playback controls. Repeat with the intended network mode. Capture both the selected media source and frames presented by the player. |
| `LocomotorLevel` | Compare movement and camera input, mouse and gamepad mapping, spawn and the return transition. |

Record state samples and event order at the same entry points on each side. For camera and UI, capture rendered frames after equivalent input and identify the input device. For diagnostic parity, record a Visual Logger session with the applicable debug control enabled and inspect the entries for the same actor, category, and event or geometry. Repeat with the control disabled. A headless map load, an injected input and a captured frame each establish different parts of the behavior.

Build Win64 Development Game and Editor, run the migration Automation suite, then cook, stage, package, and launch the reference and candidate. The imported sample configuration lists only `DefaultLevel` under `MapsToCook`. Include all five maps explicitly in the Win64 comparison. With AutomationTool `BuildCookRun`, pass `-MapsToCook=/Game/Levels/DefaultLevel+/Game/Levels/LocomotorLevel+/Game/Levels/LookAtPOI_Level+/Game/Levels/NPCLevel+/Game/Levels/RagdollLevel`. Repeat the input and playback cases that depend on cooked assets. Record the exact commands, process exit codes, packaged launch results, and any checks marked **Not run**. Generate the API reference with `doxygen Doxyfile` and follow the [Visual Studio handoff](../README.md#develop-in-visual-studio) from the candidate's `.uproject`.

## Preserve serialized and animation boundaries

| Boundary | What to preserve during integration |
| --- | --- |
| Existing Blueprint subclasses | Keep package paths, authored component templates, tuning defaults, and the fields and delegate signatures used by levels, animation, and StateTree bindings. Update native forwarding and the saved package together. |
| Character animation | Keep the AnimGraph, retargeters, rig evaluation, Choosers, Pose Search databases, and montage selection and playback with the assets. Carry the native movement snapshots through the same animation interface and update phase. |
| Linked movement and animation | Keep motion-warp targets and animation timing together. Put gameplay collision, claims, movement modes, and recovery in the native owner that can release them on completion, interruption, or teardown. |
| Smart Objects | Keep the StateTree's bindings and authored animation costs and playback gates. Bind movement and interaction ownership to the actual actor and verify claim and correction cleanup on each participating peer. |
| Sample presentation | Retain authored layouts, meshes, light and material tuning, and timelines. Preserve camera and demonstration controls used by each level when moving their state or input handling. |

## Install the runtime plugin

1. Copy `Plugins/SampleLocomotion/` into the destination project's `Plugins/`, omitting generated directories. Enable the `SampleLocomotion` and `EnhancedInput` plugins. Add `SampleLocomotion` to any C++ module that directly includes the plugin's public headers.
2. Regenerate project files if your IDE needs them and build the destination's Editor target. Keep the destination project's game mode, renderer, collision settings, map defaults, and input ownership.
3. For CMC-based characters, derive your pawn from `ASampleLocomotionCharacter` or call its movement API from your own subclass. Configure its mesh and camera in the host. Set the standing gait speeds on the class defaults and tune crouching, acceleration, braking, and rotation on its Character Movement Component. With an existing Mover pawn, keep its movement base and integrate eligible utilities independently.
4. Configure the destination to use Enhanced Input. In the pawn defaults, assign Axis2D actions for camera-relative move, world-relative move, and look as needed. Assign distinct actions for jump, crouch, walk toggle, sprint and strafe. The plugin expects X to mean right and Y to mean forward for move actions. Apply any gamepad rate scaling, axis inversion, or dead zones in your input setup. Use immediate digital actions for the hold callbacks.
5. Apply contexts in the destination's local player or PlayerController input lifecycle. Keep shared mapping contexts in their existing owner. Bind a given movement source to one of the two move actions at a time to avoid combining incompatible coordinate frames.
6. Use `USampleLocomotionAnimInstance` as the parent of a new or compatible Animation Blueprint, or capture `FSampleLocomotionSnapshot` in a host animation instance on the game thread. Use Property Access for worker-thread graph evaluation. The snapshot provides horizontal speed, signed local direction, velocity, acceleration activity, crouch and fall state. The native character also supplies gait and rotation intent.
7. For a traversal experiment, attach `USampleTraversalProbeComponent` to the pawn and pass its capsule and the desired heading. Choose a collision channel that blocks traversable geometry and headroom. Resolve the result through your own Chooser or gameplay action and recheck collision before playback. The probe does a top and destination clearance check. It does not establish a clear swept path, select a montage, drive motion warping, or manage movement mode.

## Integrate with an existing production character

1. Decide where input, authoritative movement, and interaction state live in the destination. Route input through its existing possession lifecycle. Keep one writer for movement intent and one owner for applying movement settings each tick.
2. Add an adapter from gameplay state to the Animation Blueprint's existing inputs before replacing its pawn references. Preserve the meaning of all enum values and booleans and record when each value is produced relative to movement and animation evaluation. Run any `UObject` lookups on the game thread and pass value snapshots to worker-thread graph evaluation.
3. If you are porting the sample-specific CMC pawn, map the standing and crouching speeds, strafe curve, gait, and input state to typed fields in the destination. The reference adapter resolves fields on the original imported Blueprint so its local defaults and callers can survive migration. Use the destination's own state and tuning types for a new pawn.
4. Record the existing movement and animation tick prerequisites before replacing any dispatchers. Check floor, falling, possession, and movement mode transitions in PIE. Confirm the Animation Blueprint still receives the intended frame's state when parallel animation evaluation is enabled.
5. Keep authored montage gates, phase transitions, blending, and poses in animation assets. Give the gameplay action a clear owner for claiming targets, validating movement and collision, and releasing state on interruption. Check each action at both its success boundary and its cleanup boundary.

On possession, let the local player own the input context and the player controller own the selected camera. Keep any existing possession and camera asset entry points intact when changing their implementation. If the destination sends jump and landing feedback for CMC simulated proxies, compare successive observed movement states and forward the original velocity sample to the presentation callbacks.

Keep the sandbox camera-style shortcut with the owning input component if the destination needs it. Wheel and D-pad changes share the same bounded style index and leave camera rigs and blending in the authored assets.

Bind the CMC settings update before the movement component ticks. Update rotation before choosing gait and braking. Keep the mesh dependency on the actor and attach the simulated-movement observer only to proxies so local jump and landing events still come from Character Movement.

### Preserve analog gait thresholds

Read the movement action values before applying any normalization for fixed-speed movement. The reference CMC pawn binds Enhanced Input action values during possession and treats either camera-relative or world-relative input reaching the authored magnitude threshold as full input in dual-gait modes. The authority and simulated proxies can lack a local input component, so define their behavior explicitly. If the destination predicts movement on clients, include the state required by its prediction model and verify that server corrections preserve gait and rotation transitions. Test below, at, and above the threshold, with and without walk or sprint intent.

### Replace existing input handlers

For CMC jump input, keep the authored montage slot gate and attempt traversal before issuing the usual CMC jump. The ground probe scales its reach with forward speed. Falling and flying use the shorter, taller approach. Both movement systems send the native approach to the same traversal component so the original chooser and playback callbacks can consume it. If existing graphs call `GetTraversalCheckInputs`, route that function to the same builder and preserve its typed output.

Keep the existing `Get_PropertiesForTraversal` call sites when migrating chooser inputs. The pawn owns the capsule, mesh and Motion Warping component supplied by the query. Compute horizontal speed from the movement component and read gait from the same state the old chooser used. CMC uses the current pawn gait, and Mover uses its finalized input snapshot.

Keep camera rigs connected to the original `Get_PropertiesForCamera` interface. CMC resolves aim before strafe, reads its current stance and gait, and keeps the camera struct's default movement mode. Mover reads gait and facing after simulation, lets twin-stick override facing, and converts the current movement mode through the pawn's authored mode map. Keep the camera style default for Mover. Check all five struct fields against the reference before comparing the rendered rigs.

Bind movement and look on the pawn's local input component during possession. Keep the host's mapping contexts in their existing owner and preserve each action's trigger phase, dead zone and modifiers. If moving an existing Blueprint pawn to native bindings, switch each action's handler at the same time as removing its original event chain. In the editor, verify one callback per trigger phase after possession and after a pawn swap. Test mouse look as a per-update delta and stick look as a turn rate evaluated at several frame rates.

When adapting the sample's input intent for networking, keep one authoritative state and feed the same state to camera and animation consumers. Inspect both the press and release rules on each action. The reference CMC pawn applies intent locally, sends an unreliable snapshot to the server, and replicates the shared state to observers. In the destination, verify press and release under latency and packet loss, especially the final release. Select a delivery rule that matches the destination's movement prediction and correction requirements.

### Read Mover state at the simulation boundary

The reference uses Network Prediction's `Independent` ticking policy and `Interpolated` simulated-proxy LOD. The project declares both values in `Config/DefaultNetworkPrediction.ini` so they are available during startup. Match the effective policy in the host before comparing Mover trajectories. At a 60 Hz engine cap, Fixed mode can advance simulation by a truncated 16 ms per frame while Independent mode commonly alternates between 16 and 17 ms.

For a Mover-based pawn, take horizontal speed and the current mode name from `UCharacterMoverComponent`. Translate mode names to the destination's state type before supplying them to animation and camera consumers. Preserve the exact snapshot used for gait. The sample reads pre-simulation move input and orientation intent, gives sprint priority over the walk toggle, and caps sprint in aim mode. Test the directional dot product and analog deflection at the original thresholds.

Keep the input producer and the simulated snapshot separate. Assemble one command per Mover input request, and carry crouch in the same prediction data as movement. During the pre-simulation callback, read crouch from the command being executed. Animation and camera consumers should read the latest simulated command. When porting the sample's user-defined input struct, either keep that struct and its network serialization or update every dependent movement mode and consumer together.

Bind the simulation callbacks at startup and remove them during teardown. Seed the first command's facing from the pawn's spawn rotation. Preserve the mesh tick prerequisites and the order in which the gameplay snapshots feed audio and animation presentation.

Bind the host's Enhanced Input actions during local possession. Sample the bound movement value when producing the command. The sample swaps device axes into the pawn's forward and right axes, rotates the result by control yaw, and includes control pitch in Flying mode. AI consumes its navigation request at the same boundary. Keep the aim, strafe and stance intent fields in the snapshot shared with animation.

Keep targeting registration and facing intent on the owning pawn. The sample's overlap actors use a stable interface to register candidates. The native pawn updates the nearest valid target and exposes it to the same camera and animation readers. When porting twin-stick input, retain the stick axis convention, dead zone, and control-rotation reset. Preserve the analog deflection's faster rise and slower decay when feeding gait.

Use the same pre-simulation direction limits for sector selection and the Mover debug overlay. The signed limits define where the overlay starts each arc. Keep the draw pass at the end of the pawn update and preserve both the local-control check and `DDCvar.DrawCharacterDebugShapes`. Verify the overlay against the capsule origin, root-bone facing, target orientation and simulated movement intent.

Keep the animation graphs' diagnostic calls connected to the imported debug library when moving the drawing into C++. Forward the caller's world context for scene and Visual Logger submissions. Preserve the authored line-series struct and read it without modifying the caller's array. Check the submitted lines and their order for the axes, series, legend, status text and history text.

For paired interactions, rebuild the candidate list before the animation chooser runs. Filter for eligible pawns within the overlap radius, vertical tolerance, and facing arc. Sweep the host's character collision profile between the two pawns to reject obstructed candidates. Keep the role assignments, pose search, and montage selection in the animation assets. Map the sample's object query and collision profile to named settings in the destination project.

Bind each paired action once in the owning pawn. Pass the requested action name and current candidates to the authored chooser. The sample uses the same keys for demonstration tools while aiming. Port those tool callbacks separately if the destination needs them.

Track the two capsule overrides together with the initiator's mesh tick dependency. Release the pair when either montage blends out or is interrupted, or when either pawn leaves play. Preserve existing collision ignores and tick dependencies that the pair did not install.

Route jump presses through the movement mode and traversal gate before publishing a jump edge. The sample builds separate ground and air probes for the traversal asset, which still chooses the montage. A rejected probe can request an uncrouch or publish a one-frame jump edge for prediction. Hold input can retry traversal without publishing additional jump edges. Keep ragdoll recovery tied to confirmed floor contact.

The sample falling mode extends stock Mover air movement with command-driven facing at up to 300 degrees per second. Preserve the custom command's rotation offset and verify the turn after a jump. Its slide transitions enter above 380 cm/s while crouched and exit at or below 200 cm/s or when standing. Keep both thresholds aligned with the destination's authored movement speeds.

To carry over directional movement, derive the signed heading from pre-simulation orientation intent and the current move input on the ground. Use planar velocity in air and during a slide. Carry the previous sector and offset between commands so the heading remains stable across rear-angle wraparound. Apply aim and strafe thresholds before evaluating the existing rotation-offset Chooser and its curves. Preserve the forward fallback for sprinting, stationary input and orient-to-movement.

For the sample's flight shortcut, queue a mode transition through Mover. An active flight returns to Falling. The desktop `Z` binding is for development builds, and the gamepad stick click uses the standard input binding.

Apply crouch cleanup after Mover finalizes a transition. On airborne entry, clear the stored crouch request and uncrouch when the character is crouched. On slide exit, clear the stored crouch request when sprint is held. Keep the slide exit cleanup after the original audio fade when importing the sample's presentation.

For CMC landing, publish the incoming world-space velocity before calling foley. Start the 0.3-second flag after foley dispatch. Restart the same latent action on a second landing so the earlier expiration cannot clear the new state. Keep the flag available to the animation graph through its existing character state interface.

The ragdoll test level's aimed interact action uses the first player's camera to place or rotate test targets. Supply a target pawn class and match the sample's first object-query slot to the destination's collision profile if copying the tool. The tool traces 1,000 cm, rotates a hit Mover pawn 90 degrees around yaw, or places the target 90 cm above the impact or trace end. Give each tool owner its own three-slot queue. The oldest target is retired when a fourth attempt is recorded.

The demo reticle and ragdoll hit use the evaluated Gameplay Camera child. If you port those controls, point both at the same evaluated view transform and pair the trace's fourth object-query slot with the target mesh's collision channel. Apply the authored injury preset before adding the velocity-change impulse to the hit bone. The sample keeps montage blend-out and impact presentation in the authored ragdoll flow.

The sample ground modes use the original smooth walking solver. Carry over the walk, run, sprint, and crouch speeds, along with their braking and facing settings. The walking mode preserves the short landing brake window and can align input velocity with the floor. The sliding mode uses a timed entry boost and slope-dependent speed and braking. Keep the mode instance settings with the pawn so changed tuning follows the same simulation path.

For Flying, keep Mover's stock flight solver and apply the sample's linear and facing damping to its movement proposal. Configure the speed on the mode asset or with the destination's shared movement settings. For Ragdoll, feed the capsule solver a pelvis transform per simulated command. Match the downward sphere trace to the destination's ground collision channel, probe radius, and standing capsule height. Publish the resulting capsule transform without a movement base. A missing snapshot holds the capsule until the next command.

Resolve orientation before finalizing each input command. Preserve stationary aim thresholds, the last facing on a moving base, and the precedence given to root motion. Cache floor contact from the same Mover component used for simulation. Dispatch presentation callbacks only when the finalized mode changes.

Apply `migrate_mover_mode_transitions.py` before `migrate_mover_ragdoll_mode_transitions.py`. The base pawn forwards jump, landing and slide decisions and crouch cleanup to C++. It retains the Foley callbacks and the existing fallback graph. The ragdoll child forwards capsule collision on mode entry and retains its authored exit animation callback. Its installer verifies the base pawn's compiled forwarding path first.

When adapting a sandbox teleport or reset, update orientation intent alongside the actor transform. Let the host assign the next movement mode through Mover's simulation queue. Verify absolute teleports, relative orientation offsets, and the reset transform independently. Keep the host's authority and prediction policy for any replicated teleport.

For a physics-driven ragdoll, derive the command's capsule transform from the simulated pelvis and spine. Include that transform only while Mover is about to simulate the ragdoll mode. Keep the custom command type and its consumers in sync when moving this path to the host. Identify the host skeleton's pelvis and spine bones before adapting the calculation. Carry the measured center-of-mass and spine velocities separately, since the former is used for rolling direction and the latter comes directly from physics. Preserve the host's fall-speed threshold and Physics Control modifier settings when porting the gravity limit.

Use the host equivalent of the `RagdollTrace` socket for impact prediction. The native query sweeps a 20 cm sphere along the spine velocity and world gravity for up to one second at 10 Hz. Match the host collision response to the sample's PhysicsBody trace channel. Publish the last trace destination's time and the socket-local yaw and pitch through the animation interface. Keep the authored graph responsible for how those values affect the pose.

Start skeletal physics before applying an entry impulse. Request the ragdoll mode through the simulation queue, then confirm the transition on the next engine frame and restore the configured default profile if the request failed. When leaving ragdoll, restore capsule collision before capturing the recovery pose. Let the animation graph choose and play its get-up. Restore the default physics profile after the pose handoff and skip that restoration if a new ragdoll request is pending.

For rolling, combine the slope-derived force with the pawn's movement input and clamp the result to unit length. Keep the torque bone and slope thresholds aligned with the host rig and terrain. The reference NPC waits three seconds, then rechecks mode and settling speed before requesting recovery.

Route the manual ragdoll action and the obstacle trigger through the same entry callback. Keep the default physics profile independent of the active ragdoll profile so a tuning change takes effect after recovery. The sample filters Foley contacts by bone speed and normal impulse before passing gain to the authored sound event.

For Physics Control, create the controls and body modifiers from the host's authored asset during pawn initialization. Preserve the tick dependency from skeletal animation to the post-animation callback and then to Physics Control so curve-driven strengths reach the same frame.

Configure the leg body groups with the host Physics Asset's body names. The sample allows the two groups to collide in ragdoll and switches back to its `Free` joint constraint profile when animation drives the mesh. Reuse the obstacle trace only if the host supplies its object channel and neck and foot socket names. The sample sweeps from the neck to the midpoint between the feet, using a 15 cm radius and its authored Obstacle object type.

### Connect CMC ragdoll transitions

The CMC ragdoll hooks stop character movement, disable capsule collision, and simulate the pelvis and its descendants. Recovery restores the sample's pawn mesh and capsule collision settings before normal falling and landing resume. A host with different channel or capsule settings should restore its own policy at this boundary. The CMC ledge callback also clears crouch when leaving walkable ground.

`migrate_cmc_foley_volume.py` routes jump and landing gain through the native CMC pawn and retains the existing Foley event calls. Jump gain maps ground speed from 0–500 cm/s to 0.5–1.0. Landing gain maps vertical velocity from −500 to −900 cm/s to 0.5–1.5. Both mappings clamp outside their authored intervals.

### Integrate pawn selection with the host GameMode

The sandbox uses `DDCvar.PawnClass` to switch demonstration pawns for everyone in the active world. Keep the production game's existing login, spawn, and restart policies. If the host needs a pawn switch, resolve the permitted class on the authority before restarting the player. Check the spawn location and restore view rotation after possession. Keep any client-facing selection UI separate from the authoritative validation and restart.

The reference controller's gaze teleport is a local sandbox navigation command. For an online gameplay teleport in a host project, have the authority establish the destination and validate collisions, occupancy, and player permissions. Preserve the intended movement mode and test correction on an owning client for each movement component.

### Connect traversal playback to movement

Apply `migrate_traversal_native_gameplay.py` through the ordered runner after the earlier traversal migrations. It moves selection publishing, replay setup, warp target geometry, and movement and collision lifecycle into `USampleTraversalComponent`. The existing component retains its Chooser, montage calls and callbacks, and server and multicast events. Use `--verify-only` to check the saved package and its backup against the installation receipt after reopening the Editor.

At component startup, cache the owner's authored capsule and mesh interface. For the Mover path, resolve the owning pawn's Character Mover Component before binding playback. Rebuild the cache when the host replaces the traversal owner.

After geometry and montage selection succeed, publish the accepted result before starting local playback. Send the stored result through the server RPC after the local start so the network path preserves the original ownership and playback order.

Keep the montage and warp target data in the animation assets. Wire the action's start, blend-out, and interruption callbacks to the host movement system. The sample CMC path enters flying and chooses walking or falling from the traversal result. Its sample-specific replication window stays bounded by the traversal cleanup. In an authoritative host, decide which movement state the client may predict and verify restoration on every exit. Exercise target loss, montage failure, interruption, unpossession, and level travel. Keep Mover mode queues separate from CMC movement flags.

For CMC, let the current action own the correction timer. Starting another action during the grace period must cancel the previous timer. For Mover, distinguish a queued traversal mode from an active one. If a montage is interrupted before the start takes effect, cancel the pending request while preserving the current movement mode. Once traversal is active, apply the host's exit policy. Test cancellation both in the same simulation step as entry and after the mode changes.

When the owning player starts traversal locally, use the replicated result to start playback on the authority and other observers that still need it. Route acceptance, rejection, and correction to the owner separately. Use a typed action identifier in the host if replays can arrive after possession changes or if the server can initiate actions. Validate the obstacle and result against authoritative gameplay state before replaying client-supplied data.

Track temporary collision changes for the same lifetime as the action that acquired them. The sample passes the movement capsule and exact obstacle component to `BeginTraversalCollision`. The Mover path additionally supplies its visual mesh to suspend `WorldDynamic` collision. Use the same native cleanup for completion, interruption, and component teardown. Preserve any existing move-ignore entry and the previous mesh response. If a host action changes collision independently during traversal, define which system owns the affected response before layering another override.

### Author traversal ledges

Give each traversable obstacle a set of spline components, ordered consistently, and map each front spline to its opposite where applicable. Author each spline's up vector to represent its outward ledge normal in world space. Preserve spline direction, scale, and the corner clearance when porting existing levels. The sample chooses the front spline from the pawn position, projects the collision impact to that spline, clamps the point half the minimum ledge width away from either end, and samples the opposite spline from the clamped position.

Build the ledge list after parent construction has initialized the block. The sample uses the four authored components in numeric order and pairs 1 with 2 and 3 with 4 in both directions. Rebuild both collections when the block is reconstructed so spline references belong to the current instance.

Keep the query result in a typed native structure in the host project. Update it when the host's gameplay trace hits a block and feed the resulting transforms into the existing action and animation pipeline. The sample's one-call Blueprint adapter exists to preserve the imported traversal result structure. When adapting the native query, map the obstacle's ordered splines, opposites, and minimum width to the host's own types. Verify straight and curved splines, narrow edges, missing opposites, and obstacles rotated in the level.

### Validate traversal clearance before selection

Run the approach sweep on the destination channel that identifies traversable obstacles. Use the pawn capsule dimensions for the clearance sweeps. Keep the obstacle component from the approach hit, and revalidate that component before excluding it from movement collision. Allow a separate result for reaching the front ledge with the crossing blocked. Pass the front transform, usable depth, and far floor information to the existing Chooser so it can distinguish mantle, hurdle, and vault candidates.

The reference `FSampleTraversalClearanceQuery` uses the sample block class and its two trace channel indices. Map both channels to the host project's collision profiles and substitute the host's typed obstacle lookup when adapting it. Begin with a clear ledge, a blocked approach, a blocked crossing, and a missing far floor. Compare the chosen montage and final pawn location for each movement implementation that the host supports.

## Integrate Smart Object interactions

Enable StateTree, Smart Objects and Gameplay Interactions in the host. Keep the StateTree transitions, gameplay interaction assets, and Smart Object definitions together. The sample's slot tasks retain the imported Blueprint properties for the searching actor, candidate slot, interaction owner, and claim handle. Adapt these bindings if the host uses a different schema.

Use the searching actor as the filter context. Pass the selected handle to the normal-priority claim step and carry the claim into interaction and cleanup. A candidate may become unavailable before the claim is issued. Validate the claim before starting the interaction and release it on every path that owns it. Test contention and interruption separately from animation playback.

For NPC patrol, sample a reachable navigation point outside each configured interaction exclusion radius. Limit retries and use the pawn's current position when no point qualifies. Keep focus and slot alignment bound to the same interaction target, and route temporary collision changes through the existing replicated interaction component. Store cooldown expiry in the controller using world time so repeated searches for the same interaction respect the delay.

Start the NPC StateTree from the AI controller when the host is ready. The sample waits 2 seconds in local and listen-server play and 8 seconds on dedicated servers. Preserve the evaluator's actor and controller bindings. Send AI gait intent to the possessed pawn through its input snapshot so navigation, movement, and animation read the same request.

The Smart Object use step dispatches the engine's Gameplay Interaction AI task for the already claimed slot. Keep that task under the controller's lifetime while the StateTree continues into the authored interaction. Apply and remove capsule ignores through the same replicated event on entry and cleanup. If the host uses CMC, apply its temporary correction override only on the intended peers and verify that cleanup restores the previous gameplay policy.

For approach selection, measure the world-space distance to the slot and the signed bearing in the slot's local transform. The authored Chooser may use both values. On player cancellation, dispatch the existing completion request before clearing the active interaction flag, then close the playback gate.

For player interactions, configure the StateTree and matching payload type on the interaction component. Supply the player interaction gameplay tag and register interactable world geometry on the query's object type. The request accepts the nearest free slot within 300 cm from the object's front side. Keep the playback-cancellation gate tied to accepted requests and retain the StateTree's claim step to handle contention between search and use.

On a networked pawn, enable component replication and submit the owning client's interaction request to the server. The server must inspect its own authoritative pawn transform, geometry and available Smart Object slots before it begins the player StateTree. Replicate acceptance and activity state back to the owning client, opening the local playback-cancellation gate only after acceptance. When the loop gate accepts movement, forward cancellation once to authority and preserve the component's authored completion delegate. Verify that the exit finishes and the slot can be claimed again. The imported component retains `PlayMontage_Multi` for authored montage dispatch across peers. [Two-client dedicated-server evidence](ReferenceBehavior.md#dedicated-server-smart-object-authority-and-client-synchronization)

For multiplayer demonstration maps, use the server-owned `ASampleMultiplayerSpawnGameMode` placement policy when the authored PlayerStart is blocked. It first attempts the authored transform, then searches walkable floor positions with capsule clearance against other possessed players, blocking geometry and the actual trigger bounds of level-style buttons. The sandbox GameMode inherits this policy directly. The imported Locomotor GameMode receives the same native parent through `Scripts/migrate_multiplayer_spawn_game_mode.py`. Keep the `LookAtPOI_Level` spectator spawn behavior distinct because its non-colliding spectators may intentionally overlap. In the Editor, choose **Play as Client** and set the number of players under Play settings to exercise multiple owning clients and the dedicated server together.

## Port sandbox obstacles

Keep each obstacle's authored mesh, collision, and movement curve on its asset. For physics obstacles, connect the path controller to Mover as the input producer and carry over the destination's simulation settings. For timeline obstacles, preserve the same curve length, reciprocal duration, looping, and startup delay. For the zero-gravity zone, update both Mover gravity and the ragdoll physics multiplier on overlap. The sample restores its default gravity on exit. A host with stacked gravity zones should route these events through its existing gravity owner.

For an intra-level portal, bind the sender trigger to the destination's arrival component. Preserve the entering actor's rotation and let Unreal resolve collision at the arrival point. If a level designer moves or edits a destination, refresh the linked senders' labels, colors, facing, and plate widths. For targeting dummies, route both overlap edges through the host's target receiver interface so the owning pawn can add and remove candidates.

When porting a sandbox button, bind its press delegate or replace its authored console command with the destination's gameplay action. Trigger overlaps share a 0.2-second cooldown per actor. Explicit presses dispatch immediately. Keep level bindings to the button's delegate when replacing the overlap handler.

For a randomized test obstacle, capture its starting transform during setup. Sample the authored translation, rotation, and scale ranges in actor-local space and reapply each result from that baseline. Restore the same baseline on reset, and update any height label after movement so it reflects the obstacle's current scale.

If the host uses sandbox themes, keep style data and material parameter names with its art assets. Create the block's dynamic material during construction and apply either its own values or the active theme group. Apply the same theme to fog, decal and landscape consumers when it changes. The sample's Landscape API requires a landscape configured for dynamic material instances.

In multiplayer, place a replicated `ASampleLevelVisuals` controller and let authority select `StyleIndex` through `SetLevelStyle`. `ReplicatedStyleIndex` notifies each client to apply the same authored style to its local rendering assets and also initializes late joiners. The level button's overlap and explicit simulated press run on authority, with the shared theme replicated to all connections. Rendering and dynamic material instances remain client-local presentation of the server's selected index.

When porting the sandbox still camera, keep the camera component and follow/look options on its asset. Aim from the current camera location first, then translate by the watched actor's movement since the previous tick. Update the saved location even when follow is off so switching follow on preserves the current framing.

## Bring in licensed animation data

For optional character appearances, keep the selected visual actor class separate from the gameplay pawn. Resolve soft classes asynchronously and cancel superseded requests. Assign the visual actor to a tagged child component and restore the source mesh when the selected class clears. Carry the existing replication notification to the destination if remote peers must update the same appearance.

When using the Epic sample assets, acquire the sample under the applicable terms. In a scratch branch of the destination project, use Content Browser → Asset Actions → Migrate, review Asset Report dependencies, and target the destination's `Content/` directory. Review package name conflicts before accepting an overwrite. Preserve referenced package paths and enable required plugins for each migrated feature. For instance, a Motion Matching graph needs Pose Search assets and may also depend on Chooser, Motion Warping, Animation Warping, and Control Rig.

Skeleton, IK Rig, retargeter, Pose Search schema, databases and tuning curves form a coordinated set. Verify their bone names, units and curves before rebuilding the search databases. Merge any required gameplay tags, collision channels and project settings explicitly. Refer to [Epic's asset migration guide](https://dev.epicgames.com/documentation/en-us/unreal-engine/migrating-assets-in-unreal-engine).

## Port isolated examples

If you move the isolated Locomotor example, preserve its walker and animation assets. Install its Enhanced Input mapping context on the owning local player at possession or restart. Keep its normalized controller-relative movement and separate mouse and gamepad look sensitivities if you need the same feel.

If you move the Look At POI example, let the controller own the local interface and the player-to-spectator switch. Assign the host's playable pawn and existing spectator. Clear obsolete target gizmos on entry and deselect zones on return. Keep the demonstration's authored look-at graph, gesture montages, and interface layout.

Bind the possession buttons once when the widget initializes. After each controller request, show the opposite action and collapse the action just used. The patrol reset reads the currently selected animation instance to find its pawn and restores the authored patrol transform. Carry the layout's three button names and its selection fields over when reusing the sample widget parent.

In the About panel, bind About and Close during widget initialization. Keep the authored gesture options in order and apply the network-specific visibility and switcher defaults during construction. Preserve each page's localized FText and File Media Source. A zone page change updates the title and switcher before storing the source and opening it. About toggles the panel and calls Play. Close collapses the panel, calls Pause, and calls Rewind.

Keep the Look At POI level's BeginPlay and Destroyed entries in its existing Level Blueprint. Each forwards once to the native diagnostic helper, which executes `a.AnimNode.ControlRig.Debug` before `ControlRig.EnableDrawInterfaceInGame`. The installer classifies the event graph and verifies a backup of the original map before saving any migration. Apply `migrate_look_at_level_runtime.py` through the ordered runner.

Publish zone selection on the controller before notifying the animation instance and widget. Cache the zone type, assigned pawn, animation instance and optional gizmo. Reject unsupported pawn types before replacing the active selection or dispatching the authored look-at and widget callbacks.

Let one zone own the current selection and its target gizmo. Deselect the previous zone before handing the selected pawn and target to animation. Use the current world actors so zones created after startup can participate. Retain the host interface delegate and keep gizmo appearance on its asset.

Bind pointer events to the zone's authored hit components after they are initialized. Apply hover and selected colors through its dynamic materials and update an active gizmo from the assigned pawn's world position plus the authored offset.

For gizmo manipulation, sample the local cursor and wheel. Start Shift control on hover and keep it active until Shift is released. Apply cursor deltas to the target's rotation and clamp wheel scaling to the intended manipulation range. Keep the target's appearance and tooltip settings on the asset.

## Migrate native diagnostics and input displays

`migrate_gameplay_anim_notify_states.py` validates the movement-mode and collision callbacks before installing their native parents. The final `migrate_native_animation_notify_instances.py` stage replaces the imported camera shake, movement-mode and collision notify instances with concrete native instances. The stage copies the authored settings and preserves timing, links, filters and track indices by replacing the object within its existing event. The movement state queues its ending mode only if the simulation still reports the override mode. The collision state restores `CharacterCapsule` only when `bRestoreCapsuleProfile` is enabled. Verify montage completion and interruption in addition to the direct native callbacks.

Run the ordered migration with the project Editor closed. After the commandlet saves and validates the animations and finds no remaining package references, the runner verifies the saved files and moves the three legacy Blueprint packages to `Saved/BlueprintMigration/NativeAnimationNotifies/Retired/`. The same recovery root contains copies of the original animations and notify packages. Reopen the project and validate the native instances before cooking.

`migrate_shared_object_names.py` preserves the existing shared library's typed `GetObjectNames` signature and routes its body through C++. Preserve ordering, duplicates and null entries.

`migrate_camera_director_selection.py` forwards the camera director's camera-style and crouch-rig policy to C++. The authored Chooser, rig assets, activation sequence and cached camera properties remain in the director. Verify the Chooser input and crouch rig across the authored camera styles and stance transitions.

Apply `migrate_shared_draw_rotation.py` before `migrate_mover_animation_diagnostics.py`. Both stages target the CMC and Mover Animation Blueprints at their existing diagnostic call sites. The shared rotation samples player controller index zero from the current animation instance's world and uses zero rotation when the controller is absent. Keep each local-pawn and CVar gate in place. The state formatter consumes the authored enum display strings in movement mode, stance, gait and direction order. The diagnostic rows retain their original string conversion, source, array slot, and draw setup. CMC and Mover keep their authored differences in the state and Motion Matching panels.

`migrate_controller_input_display.py` updates `WB_SimpleController` in place. The editor utility still creates and removes it in PIE. The runtime sampler uses the widget's stored player controller and preserves the key order, local accumulator precision and mouse smoothing. It combines each gamepad stick with its authored keyboard or mouse contribution, keeping the strict opacity threshold and the original position clamping and translation signs. Retain the existing widget tree so the native sampler drives the same on-screen controls.

`migrate_npc_controller_debug.py` keeps the callable `Debug` function on `AIC_NPC_SmartObject` and forwards its development-only draw to the native controller. The arrow starts at the controlled pawn and follows its control rotation for 50 cm. Preserve the scene and Visual Logger draw settings and the existing StateTree schedule. During verification, invoke `Debug` on a possessed NPC with different actor and control rotations, then repeat with no pawn to check the default origin and direction.

`migrate_look_at_zone_routing.py` keeps the controller's existing zone-selection event and routes successful capture through its native implementation. The controller sets its enable flag before dispatching to the asset's authored animation-target function. Compare accepted and rejected pawns, mismatched animation instances, and the widget callback order through the real zone-selection entry point.

The installer also updates the gizmo branch of the target function. A valid gizmo contributes its location plus its right vector scaled by `50 × Scale.X`. Missing or destroyed gizmos resolve to the zero vector. The player branch still reads the authored `head` socket. When upgrading an already routed controller, the installer backs up the current package separately and keeps the original backup. Verify the nonlocal authority path with no gizmo or widget and check the PIE log for null dereferences.

The installers classify the original and installed graphs before writing package backups. Keep the original package and recorded backup together until the candidate asset is compiled, saved and verified. Rerun each installer after a successful install and confirm that its package bytes remain stable.

## Verify in the destination

Set up a separate migrated reference sandbox from the acquired sample if you need a comparison before integrating features into the host. Use the ordered migration command from the [repository setup instructions](../README.md#local-reference-workspace).

- Establish a reference capture before wiring the new code. Record the test level, movement component, input device, initial transform, and the expected state, montage, and interruption outcome for each replay.
- Build the Editor and Game targets and cook a representative map.
- Use the intended controller to compare walking, running, sprint release, crouch clearance, falling, landing, and camera-relative input with your target behavior. Repeat with digital and analog input.
- Exercise possession changes and input context changes with keys held. If your project uses multiplayer, implement and test prediction and reconciliation for gait and rotation intent before relying on them.
- Check Motion Matching selections, traversal failure paths, and retargeted characters under the destination's collision and animation configuration.
- Exercise interaction rejection and interruption before, during, and after playback. Check that input, movement mode, collision, warp targets, and any claimed slot return to their expected state on loss of possession, actor destruction, and level travel.
- Package the representative map and repeat input, asset loading, and performance checks under the target platform budget.

The plugin has no runtime dependency on the sample project or its assets. See [Reference behavior](ReferenceBehavior.md) for remaining equivalence work.
