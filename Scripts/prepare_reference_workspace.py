"""Prepare an independent, local Unreal project with a licensed reference sample."""

import argparse
import json
from pathlib import Path
import re
import shutil
import tempfile


PROJECT = "GameAnimationSample.uproject"
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


def prepare(reference, workspace_root, name):
    descriptor = reference / PROJECT
    startup_map = reference / "Content" / "Levels" / "DefaultLevel.umap"
    if not descriptor.is_file() or not startup_map.is_file() or not (reference / "Config").is_dir():
        raise ValueError("Select an installed Game Animation Sample project with its original Config and Content.")
    if reference == ROOT or ROOT in reference.parents or reference in ROOT.parents:
        raise ValueError("The installed sample must be in a directory separate from this checkout.")

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
    args = parser.parse_args()
    if not re.fullmatch(r"[A-Za-z0-9][A-Za-z0-9_-]{0,47}", args.name):
        parser.error("--name must use up to 48 ASCII letters, digits, underscores or hyphens")
    try:
        print(prepare(args.reference.resolve(), args.workspace_root, args.name))
    except (OSError, ValueError, KeyError) as error:
        parser.exit(1, f"{error}\n")


if __name__ == "__main__":
    main()
