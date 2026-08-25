# Architecture

`GameAnimationSample.uproject` selects the UE 5.9 Launcher installation and loads the `GameAnimationSample` runtime module. The `GameAnimationSample` and `GameAnimationSampleEditor` targets share that module. The module registers Unreal's default game module implementation and depends on Core, CoreUObject, and Engine. The Game and Editor startup maps resolve to the engine Open World template. Project input classes use the engine's Enhanced Input plugin.

`SampleLocomotion` owns reusable capsule movement intent, the game-thread animation snapshot, and synchronous ledge probes. Its runtime dependencies are Core, CoreUObject, Engine and Enhanced Input. It owns no input contexts, maps or animation packages. The host project owns player input contexts, character presentation, animation graphs and action execution. See the [migration guide](Migration.md) for the integration boundaries.

The [local reference workspace](../README.md#local-reference-workspace) adds the sample's configuration and packages to an isolated copy of the native project. Its original gameplay entry points continue to use their authored graphs. [Reference behavior](ReferenceBehavior.md) records the features retained in that copy and the checks needed for parity.

## Verification

GitHub Actions runs repository metadata and LFS pointer checks on hosted runners. Native CI requires an isolated Windows runner with the confirmed UE 5.9 installation and a compatible Visual Studio toolchain.

For native CI, build the Win64 Development Editor and Game targets using the commands in the README. Run registered project Automation tests once available using `UnrealEditor-Cmd.exe` with a project-specific test filter and publish the test report and build logs. Add Win64 UAT packaging when project content and release requirements are available. Keep untrusted pull requests on hosted metadata checks.

Local verification used Unreal Engine 5.9.0 build 60738873, MSVC 14.44.35229 and Windows SDK 10.0.26100.0. Both Win64 Development targets built successfully. The README's Unreal Automation filter passed both tests with exit code zero. Doxygen 1.18.0 generated the API reference with warnings treated as errors. A separate UE 5.9 project built its own Editor module against the plugin's public headers and exports. Packaged builds, rendered parity, replication and performance budgets are **Not run**.
