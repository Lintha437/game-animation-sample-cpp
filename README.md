# GameAnimationSampleCpp

GameAnimationSampleCpp is an Unreal Engine 5.9 C++ project with a portable native locomotion plugin. The project file uses the shorter name `GameAnimSampleCpp.uproject` to fit Unreal's 20-character project filename limit. A clean checkout opens the engine Open World template. A local install of the sample adds the five test scenes and their animation dependencies.

## Open and build on Windows

1. Install Unreal Engine 5.9 through Epic Games Launcher, Git LFS, and the Visual Studio C++ toolchain with a Windows SDK supported by Unreal Build Tool.
2. Clone `https://github.com/Lintha437/game-animation-sample-cpp.git` with repository access. From the checkout, run `git lfs install --local` and `git lfs pull`.
3. Open `GameAnimSampleCpp.uproject` from the repository root. Build missing modules when prompted. To generate a Visual Studio solution, use the descriptor's Generate Visual Studio project files command in Windows Explorer.
4. For command-line builds, set `UE_ROOT` to your UE 5.9 installation path and use PowerShell from the repository root.

```powershell
$project = (Resolve-Path .\GameAnimSampleCpp.uproject).Path
$build = Join-Path $env:UE_ROOT 'Engine\Build\BatchFiles\Build.bat'
& $build GameAnimationSampleCppEditor Win64 Development "-Project=$project" -WaitMutex
& $build GameAnimationSampleCpp Win64 Development "-Project=$project" -WaitMutex
```

The project uses Enhanced Input, which ships with Unreal Engine 5.9. [Architecture and verification](Docs/Architecture.md)

## Develop in Visual Studio

Install Visual Studio 2026 with Game development with C++, Desktop development with C++, .NET desktop development, the .NET 10 SDK, and a Windows SDK supported by UE 5.9. Unreal Build Tool generates machine-specific project files. Regenerate them after cloning, moving the checkout, switching engine installs, or changing modules.

In File Explorer, right-click `GameAnimSampleCpp.uproject` and select **Show more options → Generate Visual Studio project files**. Double-click `GameAnimSampleCpp.sln` or `GameAnimSampleCpp.slnx` when generation finishes. The equivalent script selects the Visual Studio 2026 format explicitly and checks all four solutions and their project references.

```powershell
./Scripts/GenerateVisualStudioProjects.ps1 -EngineRoot 'D:\Program Files\Epic Games\UE_5.9'
```

If you generated the files from File Explorer, run `./Scripts/GenerateVisualStudioProjects.ps1 -VerifyOnly` to check the existing solutions, Game and Editor targets, and C++ project references.

| Solution | Use |
| --- | --- |
| `GameAnimSampleCpp.sln` and `GameAnimSampleCpp.slnx` | Open either for C++ work. The `GameAnimSampleCpp` project contains the `GameAnimationSampleCpp` game module and the `SampleLocomotion` plugin sources and tests. |
| `Automation_GameAnimSampleCpp.sln` and `Automation_GameAnimSampleCpp.slnx` | Open either to work on the engine's .NET automation scripts, including AutomationUtils and BuildGraph. Use the game solution for the C++ project and native Unreal Automation tests. |

In the game solution, select **Development Editor** and **Win64**, set **GameAnimSampleCpp** as the startup project, and build it before launching the debugger with **F5**. From the same solution, select **Development** and **Win64** for the standalone Game target. Close Unreal Editor before a full rebuild when the Editor has the destination binaries open.

If Unreal Build Tool selects a broken or unsupported MSVC installation, use the Visual Studio Installer to install a supported toolset. The verified Win64 Editor build used MSVC `14.50.35739` and Windows SDK `10.0.26100.0`.

## Local reference workspace

Acquire the [Game Animation Sample](https://www.fab.com/listings/880e319a-a59e-4ed2-b268-b32dac7fa016) and create a project through Epic Games Launcher. From the repository root, run the following with Python 3.10 or newer.

```powershell
python Scripts/prepare_reference_workspace.py 'D:\Projects\GameAnimationSample' --install-here
```

Build the Editor target and apply the ordered migration while the destination Editor is closed. Run these commands from the repository root.

```powershell
$ue = 'D:\Program Files\Epic Games\UE_5.9'
$project = (Resolve-Path GameAnimSampleCpp.uproject).Path
& "$ue\Engine\Build\BatchFiles\Build.bat" GameAnimationSampleCppEditor Win64 Development "-Project=$project" -WaitMutex
if ($LASTEXITCODE -ne 0) { throw 'Editor build failed.' }
python Scripts/apply_reference_migrations.py $project --engine-root $ue
if ($LASTEXITCODE -ne 0) { throw 'Reference asset migration failed. Check the log reported by the script.' }
```

The importer preserves package paths, copies project settings into local Unreal configuration overrides, and protects the copied files from accidental staging through local Git excludes. To try the migration in an independent workspace, omit `--install-here` and use the printed descriptor as `$project` while running the same commands from the checkout. For Windows builds, choose a short absolute path with `--workspace-root`, such as `D:\GAS`. UnrealBuildTool's generated paths can exceed its path limit in a nested checkout. Keep the acquired source assets under their applicable license. [Fab license terms](https://www.fab.com/eula)

The runner writes per-process logs under the destination project's `Saved/Logs/ReferenceMigration-*/`. Each asset stage stores its backup under `Saved/BlueprintMigration/`. On a failed stage, review that stage's log and package before retrying.

After the migration, open the descriptor. Play `DefaultLevel` or open `LocomotorLevel`, `LookAtPOI_Level`, `NPCLevel`, and `RagdollLevel` from **Content → Levels**.

See [reference behavior](Docs/ReferenceBehavior.md) for the current parity criteria and [migration into another UE5 project](Docs/Migration.md) to integrate the plugin in a C++ project. To generate the API reference, run `doxygen Doxyfile` from the repository root and open `Saved/Doxygen/html/index.html`.

## Development

Start work from `develop` and submit focused branches to `develop`. Releases merge into `main` and back into `develop`. Hotfixes start from `main` and merge back into `main`, `develop`, and any active release branch. Retain merge ancestry when back-merging.

Run `python Scripts/validate_repository.py --project GameAnimSampleCpp` against the Git index and `git lfs fsck --pointers HEAD` against the committed tree. If the remote supports Git LFS locking, coordinate binary edits with `git lfs lock <path>`, inspect locks with `git lfs locks`, and release them with `git lfs unlock <path>`.

Run the native Unreal Automation tests after building the Editor target.

```powershell
$editor = Join-Path $env:UE_ROOT 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
& $editor $project -Unattended -NullRHI -NoSound '-ExecCmds=Automation RunTests SampleLocomotion' '-TestExit=Automation Test Queue Empty'
```

Check the log for two successful test completions, `SampleLocomotion.Character.NativeMovement` and `SampleLocomotion.Traversal.GeometryAndClearance`.

A project license has yet to be established. Verify the applicable rights before redistributing sample assets or third-party material.
