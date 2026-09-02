# Migrate Sample Locomotion into another UE5 project

Use Unreal Engine 5.9 and a C++ project built with the associated toolchain. Start with the portable plugin if your project already owns possession, input, and animation. Use the licensed reference sandbox as a comparison level if you need to carry over the sample's authored animation stack.

| Source | Destination responsibility |
| --- | --- |
| `SampleLocomotion` runtime plugin | Install the plugin and integrate its CMC pawn, animation snapshot, or traversal geometry probe independently. |
| Project runtime module `GameAnimationSampleCpp` | Review the sample-specific character, tick, notify, and travel adapters before porting any of them. Their existing integrations use imported sample package paths and interfaces. |
| Locally imported Epic packages | Acquire and migrate the licensed assets separately. Keep the authored animation interfaces and verify each asset's dependencies in the destination. |

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

### Preserve analog gait thresholds

Read the movement action values before applying any normalization for fixed-speed movement. The reference CMC pawn binds Enhanced Input action values during possession and treats either camera-relative or world-relative input reaching the authored magnitude threshold as full input in dual-gait modes. The authority and simulated proxies can lack a local input component, so define their behavior explicitly. If the destination predicts movement on clients, include the state required by its prediction model and verify that server corrections preserve gait and rotation transitions. Test below, at, and above the threshold, with and without walk or sprint intent.

### Replace existing input handlers

Bind movement and look on the pawn's local input component during possession. Keep the host's mapping contexts in their existing owner and preserve each action's trigger phase, dead zone and modifiers. If moving an existing Blueprint pawn to native bindings, switch each action's handler at the same time as removing its original event chain. In the editor, verify one callback per trigger phase after possession and after a pawn swap. Test mouse look as a per-update delta and stick look as a turn rate evaluated at several frame rates.

When adapting the sample's input intent for networking, keep one authoritative state and feed the same state to camera and animation consumers. Inspect both the press and release rules on each action. The reference CMC pawn applies intent locally, sends an unreliable snapshot to the server, and replicates the shared state to observers. In the destination, verify press and release under latency and packet loss, especially the final release. Select a delivery rule that matches the destination's movement prediction and correction requirements.

## Bring in licensed animation data

When using the Epic sample assets, acquire the sample under the applicable terms. In a scratch branch of the destination project, use Content Browser → Asset Actions → Migrate, review Asset Report dependencies, and target the destination's `Content/` directory. Review package name conflicts before accepting an overwrite. Preserve referenced package paths and enable required plugins for each migrated feature. For instance, a Motion Matching graph needs Pose Search assets and may also depend on Chooser, Motion Warping, Animation Warping, and Control Rig.

Skeleton, IK Rig, retargeter, Pose Search schema, databases and tuning curves form a coordinated set. Verify their bone names, units and curves before rebuilding the search databases. Merge any required gameplay tags, collision channels and project settings explicitly. Refer to [Epic's asset migration guide](https://dev.epicgames.com/documentation/en-us/unreal-engine/migrating-assets-in-unreal-engine).

## Verify in the destination

- Establish a reference capture before wiring the new code. Record the test level, movement component, input device, initial transform, and the expected state, montage, and interruption outcome for each replay.
- Build the Editor and Game targets and cook a representative map.
- Use the intended controller to compare walking, running, sprint release, crouch clearance, falling, landing, and camera-relative input with your target behavior. Repeat with digital and analog input.
- Exercise possession changes and input context changes with keys held. If your project uses multiplayer, implement and test prediction and reconciliation for gait and rotation intent before relying on them.
- Check Motion Matching selections, traversal failure paths, and retargeted characters under the destination's collision and animation configuration.
- Exercise interaction rejection and interruption before, during, and after playback. Check that input, movement mode, collision, warp targets, and any claimed slot return to their expected state on loss of possession, actor destruction, and level travel.
- Package the representative map and repeat input, asset loading, and performance checks under the target platform budget.

The plugin has no runtime dependency on the sample project or its assets. See [Reference behavior](ReferenceBehavior.md) for remaining equivalence work.
