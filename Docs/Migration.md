# Migrate Sample Locomotion into another UE5 project

The source plugin provides a native CMC character, a game-thread animation snapshot, and a traversal geometry probe. The reference sandbox is a separate, local test project. Use Unreal Engine 5.9 and a C++ project built with the associated toolchain.

## Install the runtime plugin

1. Copy `Plugins/SampleLocomotion/` into the destination project's `Plugins/`, omitting generated directories. Enable the `SampleLocomotion` and `EnhancedInput` plugins. Add `SampleLocomotion` to any C++ module that directly includes the plugin's public headers.
2. Regenerate project files if your IDE needs them and build the destination's Editor target. Keep the destination project's game mode, renderer, collision settings, map defaults, and input ownership.
3. For CMC-based characters, derive your pawn from `ASampleLocomotionCharacter` or call its movement API from your own subclass. Configure its mesh and camera in the host. Set the standing gait speeds on the class defaults and tune crouching, acceleration, braking, and rotation on its Character Movement Component. With an existing Mover pawn, keep its movement base and integrate eligible utilities independently.
4. Configure the destination to use Enhanced Input. In the pawn defaults, assign Axis2D actions for camera-relative move, world-relative move, and look as needed. Assign distinct actions for jump, crouch, walk toggle, sprint and strafe. The plugin expects X to mean right and Y to mean forward for move actions. Apply any gamepad rate scaling, axis inversion, or dead zones in your input setup. Use immediate digital actions for the hold callbacks.
5. Apply contexts in the destination's local player or PlayerController input lifecycle. Keep shared mapping contexts in their existing owner. Bind a given movement source to one of the two move actions at a time to avoid combining incompatible coordinate frames.
6. Use `USampleLocomotionAnimInstance` as the parent of a new or compatible Animation Blueprint, or capture `FSampleLocomotionSnapshot` in a host animation instance on the game thread. Use Property Access for worker-thread graph evaluation. The snapshot provides horizontal speed, signed local direction, velocity, acceleration activity, crouch and fall state. The native character also supplies gait and rotation intent.
7. For a traversal experiment, attach `USampleTraversalProbeComponent` to the pawn and pass its capsule and the desired heading. Choose a collision channel that blocks traversable geometry and headroom. Resolve the result through your own Chooser or gameplay action and recheck collision before playback. The probe does a top and destination clearance check. It does not establish a clear swept path, select a montage, drive motion warping, or manage movement mode.

## Bring in licensed animation data

When using the Epic sample assets, acquire the sample under the applicable terms. In a scratch branch of the destination project, use Content Browser → Asset Actions → Migrate, review Asset Report dependencies, and target the destination's `Content/` directory. Review package name conflicts before accepting an overwrite. Preserve referenced package paths and enable required plugins for each migrated feature. For instance, a Motion Matching graph needs Pose Search assets and may also depend on Chooser, Motion Warping, Animation Warping, and Control Rig.

Skeleton, IK Rig, retargeter, Pose Search schema, databases and tuning curves form a coordinated set. Verify their bone names, units and curves before rebuilding the search databases. Merge any required gameplay tags, collision channels and project settings explicitly. Refer to [Epic's asset migration guide](https://dev.epicgames.com/documentation/en-us/unreal-engine/migrating-assets-in-unreal-engine).

## Verify in the destination

- Build the Editor and Game targets and cook a representative map.
- Use the intended controller to compare walking, running, sprint release, crouch clearance, falling, landing, and camera-relative input with your target behavior. Repeat with digital and analog input.
- Exercise possession changes and input context changes with keys held. If your project uses multiplayer, implement and test prediction and reconciliation for gait and rotation intent before relying on them.
- Check Motion Matching selections, traversal failure paths, and retargeted characters under the destination's collision and animation configuration.
- Package the representative map and repeat input, asset loading, and performance checks under the target platform budget.

The plugin has no runtime dependency on the sample project or its assets. See [Reference behavior](ReferenceBehavior.md) for remaining equivalence work.
