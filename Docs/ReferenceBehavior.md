# Reference behavior and parity criteria

The local Game Animation Sample is a content project. Its descriptor declares no project C++ modules. The existing reference log identifies Unreal Engine 5.9.0 build 60738873 and reports both `GM_Sandbox_C` and `GM_Sandbox_Ragdoll_C` loading. The Content directory has 3,890 .uasset packages, five .umap packages, and three movies. The startup package is `/Game/Levels/DefaultLevel`.

Prepare an isolated workspace as described in the [README](../README.md). Its map, Blueprint and asset package paths match the installed sample. Use a separate workspace for each comparison so the installed sample stays read-only.

| Area | Local evidence | Native implementation and parity criterion |
| --- | --- | --- |
| Capsule locomotion | `SandboxCharacter_CMC`, `IMC_Sandbox` | The plugin implements camera/world-space input, walk/run/sprint, jump, crouch, and strafe on CMC. Compare the sample's input timing, analog response, acceleration, direction, camera and gait transitions after wiring the native pawn. |
| Mover and ragdoll | `SandboxCharacter_Mover`, `SandboxCharacter_Mover_Ragdoll`, `MovementModes`, `RagdollLevel` | Reference avatars and modes remain. Compare slide entry/exit, transitions through air, ragdoll start, recovery, and movement prediction. |
| Animation selection | CMC and Mover Animation Blueprints and Chooser/Pose Search assets | The native snapshot provides inputs for an authored graph. Validate pose-search selections, trajectory, blending, root motion, locomotion layering, and IK in an integrated graph. |
| Traversal and paired actions | `AC_TraversalLogic`, traversal actions and montage notify states | Native geometry probe covers a facing surface, reachable top and standing clearance. Compare traversal variants, aborts, montage alignment, moving targets, and blend-out after integration. |
| Cameras and input devices | `Blueprints/Cameras`, `IA_Look_Gamepad`, `TIS_MobileControls` and `IMC_Sandbox` | Reference assets remain. Check camera modes, gamepad, keyboard/mouse, touch, aiming, twin-stick and camera collision against the reference. |
| NPCs and Smart Objects | `NPCLevel`, StateTrees, Smart Object claim/use tasks and Bench assets | Reference behavior remains. Test patrol, claim contention, paired animation and cleanup after interruption. |
| Retargeting and presentation | Retargeted character Blueprints, MetaHumans, camera rigs, widgets and audio | Retain skeletal data and authored graph assets. Test pawn/visual switching, retargeted poses, HUD/debug controls, foley and foot placement. |
| Isolated examples | `LocomotorLevel`, `LookAtPOI_Level`, `IsolatedExamples` | Reference behavior remains. Verify each example's controls and transitions and its travel back to the sandbox. |

The copied sample continues to run its existing gameplay Blueprints. The plugin is loaded alongside them. A parity comparison still requires binding the native implementation to a test pawn and comparing against the sample under equivalent inputs.

The isolated workspace built its Win64 Development Editor target. In unattended `UnrealEditor-Cmd.exe -game -NullRHI` smoke runs, all five level worlds started for play and exited with code zero. A content-only control run of `DefaultLevel` reproduced the 39 widget Blueprint compiler messages in the native workspace with no difference in the compiler messages. Headless startup verifies package loading and world initialization. Rendered gameplay and widget behavior are **Not run**.

SHA-256 comparison matched all 3,899 files under the installed sample's `Content/` and `Build/` directories with the isolated workspace. On this machine, selecting `-CompilerVersion=14.44.35207` avoided a missing linker library in another installed toolchain.

## Retained authoring

- Animation Blueprints, Pose Search, Chooser, curves, IK rigs, retargeters and Control Rigs encode authored pose behavior and dependencies. Preserve their interfaces while migrating reusable calculations.
- Gameplay Camera rigs, widget layout, sound data, level geometry, input actions and mapping contexts stay authorable. Move reusable runtime orchestration to native code only after verifying the graph's runtime inputs and outputs.
- The existing pawn, controller, traversal and NPC gameplay graphs remain in the local workspace. The portable plugin currently implements the interfaces documented in the [migration guide](Migration.md).

See the [Epic sample documentation](https://dev.epicgames.com/documentation/unreal-engine/game-animation-sample-project-in-unreal-engine) for the sample's public controls and architecture.
