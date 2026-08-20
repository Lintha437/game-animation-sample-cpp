# Architecture

`GameAnimationSample.uproject` selects the UE 5.9 Launcher installation and loads the `GameAnimationSample` runtime module. The `GameAnimationSample` and `GameAnimationSampleEditor` targets share that module. The module registers Unreal's default game module implementation and depends on Core, CoreUObject, and Engine. The Game and Editor startup maps resolve to the engine Open World template.

Gameplay migration will follow a reference inventory. Shared gameplay and animation behavior should live in C++ wherever practical. Retained asset-driven graphs should have a documented authoring or runtime requirement.

## Verification

GitHub Actions runs repository metadata and LFS pointer checks on hosted runners. Native CI requires an isolated Windows runner with the confirmed UE 5.9 installation and a compatible Visual Studio toolchain.

For native CI, build the Win64 Development Editor and Game targets using the commands in the README. Run registered project Automation tests once available using `UnrealEditor-Cmd.exe` with a project-specific test filter and publish the test report and build logs. Add Win64 UAT packaging when project content and release requirements are available. Keep untrusted pull requests on hosted metadata checks.
