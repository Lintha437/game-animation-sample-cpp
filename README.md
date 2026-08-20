# GameAnimationSample

GameAnimationSample is an Unreal Engine 5.9 C++ project. The initial project contains a native runtime module and boots to the engine Open World template map.

## Open and build on Windows

1. Install Unreal Engine 5.9 through Epic Games Launcher, Git LFS, and the Visual Studio C++ toolchain with a Windows SDK supported by Unreal Build Tool.
2. From a Git clone, run `git lfs install --local` and `git lfs pull`. Run `git remote -v` to check whether a remote is configured.
3. Open `GameAnimationSample.uproject` from the repository root. Build missing modules when prompted. To generate a Visual Studio solution, use the descriptor's Generate Visual Studio project files command in Windows Explorer.
4. For command-line builds, set `UE_ROOT` to your UE 5.9 installation path and use PowerShell from the repository root.

```powershell
$project = (Resolve-Path .\GameAnimationSample.uproject).Path
$build = Join-Path $env:UE_ROOT 'Engine\Build\BatchFiles\Build.bat'
& $build GameAnimationSampleEditor Win64 Development "-Project=$project" -WaitMutex
& $build GameAnimationSample Win64 Development "-Project=$project" -WaitMutex
```

The project uses Enhanced Input, which ships with Unreal Engine 5.9. [Architecture and verification](Docs/Architecture.md)

## Development

Start work from `develop` and submit focused branches to `develop`. Releases merge into `main` and back into `develop`. Hotfixes start from `main` and merge back into `main`, `develop`, and any active release branch. Retain merge ancestry when back-merging.

Run `python Scripts/validate_repository.py --project GameAnimationSample` against the Git index and `git lfs fsck --pointers HEAD` against the committed tree. If the remote supports Git LFS locking, coordinate binary edits with `git lfs lock <path>`, inspect locks with `git lfs locks`, and release them with `git lfs unlock <path>`.

A project license has yet to be established. Verify the applicable rights before redistributing sample assets or third-party material.
