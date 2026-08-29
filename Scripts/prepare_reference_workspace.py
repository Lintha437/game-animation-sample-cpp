"""Prepare an independent, local Unreal project with a licensed reference sample."""

import argparse
import filecmp
import json
from pathlib import Path
import re
import shutil
import subprocess
import tempfile


PROJECT = "GameAnimSampleCpp.uproject"
REFERENCE_PROJECT = "GameAnimationSample.uproject"
ROOT = Path(__file__).resolve().parents[1]


def read_project(path):
    with path.open(encoding="utf-8-sig") as handle:
        return json.load(handle)


def copy_source(source, destination):
    shutil.copytree(
        source,
        destination,
        ignore=shutil.ignore_patterns("Binaries", "Intermediate", "Saved", "DerivedDataCache"),
    )


def check_reference(reference):
    descriptor = reference / REFERENCE_PROJECT
    startup_map = reference / "Content" / "Levels" / "DefaultLevel.umap"
    if not descriptor.is_file() or not startup_map.is_file() or not (reference / "Config").is_dir():
        raise ValueError("Select an installed Game Animation Sample project with its original Config and Content.")
    if reference == ROOT or ROOT in reference.parents or reference in ROOT.parents:
        raise ValueError("The installed sample must be in a directory separate from this checkout.")
    return descriptor


def install_locally(reference):
    check_reference(reference)
    assets = sorted(path.relative_to(reference) for path in (reference / "Content").rglob("*") if path.is_file())
    configs = {}
    for path in (reference / "Config").glob("Default*.ini"):
        data = path.read_text(encoding="utf-8-sig")
        if path.name == "DefaultEngine.ini":
            data = re.sub(r"(?ms)^\[/Script/AndroidFileServerEditor\.AndroidFileServerRuntimeSettings\]\n.*?(?=^\[|\Z)", "", data)
        if path.name == "DefaultGame.ini":
            data = re.sub(r"(?m)^(?:ProjectID|ProjectName)=[^\n]*\n?", "", data)
        configs[Path("Config") / ("User" + path.name.removeprefix("Default"))] = data.encode("utf-8")

    tracked = subprocess.check_output(
        ["git", "ls-files", "-z", "--", "Content", "Config"], cwd=ROOT, encoding="utf-8"
    ).split("\0")
    paths = set(assets) | configs.keys()
    if any(not (ROOT / path).resolve().is_relative_to(ROOT) for path in paths):
        raise ValueError("An imported path resolves outside this checkout.")
    conflicts = {Path(path) for path in tracked} & paths
    if conflicts:
        raise ValueError(f"An imported path is tracked: {min(conflicts)}")

    for path in assets:
        source = reference / path
        destination = ROOT / path
        if destination.exists() and not filecmp.cmp(source, destination, shallow=False):
            raise ValueError(f"An existing local asset differs: {path}")
    for path, data in configs.items():
        destination = ROOT / path
        if destination.exists() and destination.read_bytes() != data:
            raise ValueError(f"An existing local configuration differs: {path}")

    exclude = Path(subprocess.check_output(
        ["git", "rev-parse", "--path-format=absolute", "--git-path", "info/exclude"],
        cwd=ROOT, encoding="utf-8"
    ).strip())
    exclude.parent.mkdir(parents=True, exist_ok=True)
    current = set(exclude.read_text(encoding="utf-8").splitlines()) if exclude.exists() else set()
    patterns = [
        "/" + re.sub(r"([\\\[\]*?])", r"\\\1", path.as_posix()) for path in sorted(paths)
    ]
    with exclude.open("a", encoding="utf-8") as handle:
        handle.write("".join("\n" + pattern for pattern in patterns if pattern not in current))
        handle.write("\n")

    for path in assets:
        destination = ROOT / path
        if not destination.exists():
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(reference / path, destination)
    for path, data in configs.items():
        destination = ROOT / path
        if not destination.exists():
            destination.write_bytes(data)
    return ROOT / PROJECT


def prepare(reference, workspace_root, name):
    descriptor = check_reference(reference)

    workspace_root = workspace_root.resolve()
    generated_root = (ROOT / "Saved" / "ReferenceWorkspaces").resolve()
    inside_checkout = workspace_root == ROOT or ROOT in workspace_root.parents
    inside_generated = workspace_root == generated_root or generated_root in workspace_root.parents
    if workspace_root == reference or reference in workspace_root.parents:
        raise ValueError("The workspace directory must be outside the installed sample.")
    if inside_checkout and not inside_generated:
        raise ValueError("Inside this checkout, place workspaces under Saved/ReferenceWorkspaces.")
    workspace_root.mkdir(parents=True, exist_ok=True)
    destination = workspace_root / name
    if destination.exists() or destination.is_symlink():
        raise FileExistsError(f"{destination} already exists. Choose a different --name.")

    native_project = read_project(ROOT / PROJECT)
    sample_project = read_project(descriptor)
    plugins = {plugin["Name"]: plugin for plugin in sample_project.get("Plugins", [])}
    plugins.update({plugin["Name"]: plugin for plugin in native_project.get("Plugins", [])})
    native_project["Plugins"] = list(plugins.values())

    with tempfile.TemporaryDirectory(prefix=".stage-", dir=workspace_root) as temp:
        staging = Path(temp) / name
        staging.mkdir()
        shutil.copytree(reference / "Content", staging / "Content")
        shutil.copytree(reference / "Config", staging / "Config")
        if (reference / "Build").is_dir():
            shutil.copytree(reference / "Build", staging / "Build")
        if (reference / "Plugins").is_dir():
            copy_source(reference / "Plugins", staging / "Plugins")
        for plugin in (ROOT / "Plugins").iterdir():
            if plugin.is_dir() and (plugin / f"{plugin.name}.uplugin").is_file():
                copy_source(plugin, staging / "Plugins" / plugin.name)
        copy_source(ROOT / "Source", staging / "Source")
        (staging / PROJECT).write_text(json.dumps(native_project, indent=2) + "\n", encoding="utf-8")
        staging.rename(destination)

    return destination / PROJECT


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("reference", type=Path, help="Directory of your locally installed Game Animation Sample.")
    parser.add_argument("--workspace-root", type=Path, default=Path(tempfile.gettempdir()) / "GAS",
                        help="Parent of the new workspace. Choose a short path for Windows builds.")
    parser.add_argument("--name", default="Reference", help="Name of the new workspace.")
    parser.add_argument("--install-here", action="store_true",
                        help="Copy sample assets and local configuration into this checkout.")
    args = parser.parse_args()
    if not re.fullmatch(r"[A-Za-z0-9][A-Za-z0-9_-]{0,47}", args.name):
        parser.error("--name must use up to 48 ASCII letters, digits, underscores or hyphens")
    try:
        reference = args.reference.resolve()
        print(install_locally(reference) if args.install_here else prepare(reference, args.workspace_root, args.name))
    except (OSError, ValueError, KeyError, subprocess.CalledProcessError) as error:
        parser.exit(1, f"{error}\n")


if __name__ == "__main__":
    main()
