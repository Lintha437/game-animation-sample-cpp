# GameAnimationSample

GameAnimationSample is an Unreal Engine 5.9 C++ project. The checkout includes a portable native locomotion plugin and boots to the engine Open World template map.

## Open and build on Windows

1. Install Unreal Engine 5.9 through Epic Games Launcher, Git LFS, and the Visual Studio C++ toolchain with a Windows SDK supported by Unreal Build Tool.
2. Clone `https://github.com/Lintha437/game-animation-sample-cpp.git` with repository access. From the checkout, run `git lfs install --local` and `git lfs pull`.
3. Open `GameAnimationSample.uproject` from the repository root. Build missing modules when prompted. To generate a Visual Studio solution, use the descriptor's Generate Visual Studio project files command in Windows Explorer.
4. For command-line builds, set `UE_ROOT` to your UE 5.9 installation path and use PowerShell from the repository root.

```powershell
$project = (Resolve-Path .\GameAnimationSample.uproject).Path
$build = Join-Path $env:UE_ROOT 'Engine\Build\BatchFiles\Build.bat'
& $build GameAnimationSampleEditor Win64 Development "-Project=$project" -WaitMutex
& $build GameAnimationSample Win64 Development "-Project=$project" -WaitMutex
```

The project uses Enhanced Input, which ships with Unreal Engine 5.9. [Architecture and verification](Docs/Architecture.md)

## Develop in Visual Studio

Install Visual Studio 2026 with Game development with C++, Desktop development with C++, .NET desktop development, the .NET 10 SDK, and a Windows SDK supported by UE 5.9. Unreal Build Tool generates machine-specific project files. Regenerate them after cloning, moving the checkout, switching engine installs, or changing modules.

In File Explorer, right-click `GameAnimationSample.uproject` and select **Show more options → Generate Visual Studio project files**. Double-click `GameAnimationSample.sln` or `GameAnimationSample.slnx` when generation finishes. The equivalent script selects the Visual Studio 2026 format explicitly and checks all four solutions and their project references.

```powershell
./Scripts/GenerateVisualStudioProjects.ps1 -EngineRoot 'D:\Program Files\Epic Games\UE_5.9'
```

| Solution | Use |
| --- | --- |
| `GameAnimationSample.sln` and `GameAnimationSample.slnx` | Open either for C++ work. The `GameAnimationSample` project contains the game module and the `SampleLocomotion` plugin sources and tests. |
| `Automation_GameAnimationSample.sln` and `Automation_GameAnimationSample.slnx` | Open either for Unreal AutomationTool and its .NET projects. Native Unreal Automation tests live in the game solution. |

In the game solution, select **Development Editor** and **Win64**, set **GameAnimationSample** as the startup project, and build it before launching the debugger with **F5**. From the same solution, select **Development** and **Win64** for the standalone Game target. Close Unreal Editor before a full rebuild when the Editor has the destination binaries open.

If Unreal Build Tool selects a broken or unsupported MSVC installation, use the Visual Studio Installer to install a supported toolset. The local Win64 build used toolchain family `14.44.35207` and Windows SDK `10.0.26100.0`. To reproduce that selection in a command-line build, append `'-CompilerVersion=14.44.35207'` after installing that family.

## Local reference workspace

Acquire the [Game Animation Sample](https://www.fab.com/listings/880e319a-a59e-4ed2-b268-b32dac7fa016) and create a project through Epic Games Launcher. From the repository root, run the following with Python 3.10 or newer.

```powershell
python Scripts/prepare_reference_workspace.py 'D:\Projects\GameAnimationSample'
```

Open the descriptor printed by the script. It defaults to `GAS/Reference/GameAnimationSample.uproject` under your temporary directory to keep Windows build paths short. Use the Win64 build commands above with that descriptor as `$project`. For a persistent workspace, pass a short destination such as `--workspace-root D:\GAS`. Use `--name` for separate copies when testing changes. Keep the acquired source assets under their applicable license. [Fab license terms](https://www.fab.com/eula)

See [reference behavior](Docs/ReferenceBehavior.md) for the current parity criteria and [migration into another UE5 project](Docs/Migration.md) to integrate the plugin in a C++ project. To generate the API reference, run `doxygen Doxyfile` from the repository root and open `Saved/Doxygen/html/index.html`.

## Development

Start work from `develop` and submit focused branches to `develop`. Releases merge into `main` and back into `develop`. Hotfixes start from `main` and merge back into `main`, `develop`, and any active release branch. Retain merge ancestry when back-merging.

Run `python Scripts/validate_repository.py --project GameAnimationSample` against the Git index and `git lfs fsck --pointers HEAD` against the committed tree. If the remote supports Git LFS locking, coordinate binary edits with `git lfs lock <path>`, inspect locks with `git lfs locks`, and release them with `git lfs unlock <path>`.

Run the native Unreal Automation tests after building the Editor target.

```powershell
$editor = Join-Path $env:UE_ROOT 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
& $editor $project -Unattended -NullRHI -NoSound '-ExecCmds=Automation RunTests SampleLocomotion' '-TestExit=Automation Test Queue Empty'
```

Check the log for two successful test completions, `SampleLocomotion.Character.NativeMovement` and `SampleLocomotion.Traversal.GeometryAndClearance`.

A project license has yet to be established. Verify the applicable rights before redistributing sample assets or third-party material.
