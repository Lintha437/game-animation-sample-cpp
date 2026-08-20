"""Check the Git index of a UE project without opening the editor."""

import argparse
import json
from pathlib import Path, PurePosixPath
import re
import subprocess


def git(*args, data=None, allowed=(0,)):
    result = subprocess.run(["git", *args], input=data, capture_output=True)
    if result.returncode not in allowed:
        raise RuntimeError(result.stderr.decode("utf-8", errors="replace").strip() or "git " + " ".join(args))
    return result.stdout


def nul_fields(data):
    return data.decode("utf-8").rstrip("\0").split("\0") if data else []


def nul_input(paths):
    return ("\0".join(paths) + "\0").encode("utf-8")


def index_text(path):
    return git("cat-file", "blob", f":{path}").decode("utf-8-sig")


def ignored(paths):
    return nul_fields(git("check-ignore", "--no-index", "-z", "--stdin",
                          data=nul_input(paths), allowed=(0, 1)))


def check_project(project, paths, bootstrap, errors):
    descriptor = f"{project}.uproject"
    descriptors = {path for path in paths if "/" not in path and path.endswith(".uproject")}
    if bootstrap:
        if descriptors:
            errors.append("Root .uproject found. Remove --bootstrap from CI in the same change.")
        return
    if descriptor not in paths:
        errors.append(f"Missing indexed project descriptor {descriptor}")
        return
    try:
        document = json.loads(index_text(descriptor))
        if not isinstance(document, dict) or type(document.get("FileVersion")) is not int or document["FileVersion"] < 1:
            raise ValueError("FileVersion must be a positive integer")
        if "EngineAssociation" in document and not isinstance(document["EngineAssociation"], str):
            raise ValueError("EngineAssociation must be a string")
        build_files = {PurePosixPath(path).name for path in paths if path.startswith("Source/") and path.endswith(".Build.cs")}
        for field in ("Modules", "Plugins"):
            entries = document.get(field, [])
            if not isinstance(entries, list):
                raise ValueError(f"{field} must be an array")
            for entry in entries:
                if not isinstance(entry, dict) or not isinstance(entry.get("Name"), str) or not entry["Name"]:
                    raise ValueError(f"Each {field} entry must have a Name")
                if field == "Modules":
                    if not isinstance(entry.get("Type"), str) or not entry["Type"]:
                        raise ValueError(f"Module {entry['Name']} must have a Type")
                    if f"{entry['Name']}.Build.cs" not in build_files:
                        errors.append(f"Module {entry['Name']} has no indexed Build.cs under Source/")
    except (UnicodeError, ValueError) as exc:
        errors.append(f"{descriptor}: {exc}")


def check_ignore_and_lfs(project, paths, errors):
    keep = [
        f"{project}.uproject", "Config/DefaultEngine.ini", "Source/Module/Module.Build.cs",
        "Content/Maps/Example.umap", "Content/__ExternalActors__/Maps/Actor.uasset",
        "Content/__ExternalObjects__/Maps/Object.uasset", "Build/Windows/Application.ico",
        "Plugins/Example/Example.uplugin", "Plugins/Example/Source/Module/Module.Build.cs",
    ]
    generated = [
        f"{folder}/{name}/ue-ci-probe"
        for folder in (".", "Plugins/Example")
        for name in ("Binaries", "Intermediate", "Saved", "DerivedDataCache")
    ]
    generated = [path.removeprefix("./") for path in generated]
    for path in ignored(keep):
        errors.append(f"Project input is ignored: {path}")
    for path in sorted(set(generated) - set(ignored(generated))):
        errors.append(f"Missing generated-file ignore rule: {path}")
    if paths:
        for path in ignored(sorted(paths)):
            errors.append(f"Indexed path matches an ignore rule: {path}")

    probes = {
        "Content/Maps/Example.umap", "Content/Example.uasset", "Content/Example.ubulk",
        "Plugins/Example/Content/Example.uasset",
    }
    asset_paths = {path for path in paths if PurePosixPath(path).suffix.lower() in {".umap", ".uasset", ".ubulk"}}
    attrs = ("filter", "diff", "merge", "text")
    result = nul_fields(git("check-attr", "--cached", "-z", "--stdin", *attrs,
                            data=nul_input(sorted(probes | asset_paths))))
    expected = {"filter": "lfs", "diff": "lfs", "merge": "lfs", "text": "unset"}
    for path, attr, value in zip(result[::3], result[1::3], result[2::3]):
        if value != expected[attr]:
            errors.append(f"{path}: expected {attr}={expected[attr]}, found {value}")


def check_text(paths, errors):
    for path in sorted(paths):
        is_config = path.startswith("Config/") and path.endswith(".ini")
        is_cpp = path.startswith("Source/") and PurePosixPath(path).suffix in {".cpp", ".h", ".hpp", ".inl"}
        if not (is_config or is_cpp):
            continue
        try:
            text = index_text(path)
        except UnicodeError as exc:
            errors.append(f"{path}: expected UTF-8 text ({exc})")
            continue
        if "\0" in text:
            errors.append(f"{path}: unexpected NUL in text file")
        for number, line in enumerate(text.splitlines(), 1):
            if is_config and line.lstrip().startswith("[") and not re.match(r"^\s*\[[^\[\]]+\]\s*(?:[;#].*)?$", line):
                errors.append(f"{path}:{number}: malformed INI section")
            if is_cpp and (line.rstrip() != line or re.match(r"^(?:<{7}|>{7})(?: |$)", line)):
                errors.append(f"{path}:{number}: trailing whitespace or conflict marker")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--project", required=True, help="Exact .uproject basename without the extension")
    parser.add_argument("--bootstrap", action="store_true", help="Validate a baseline before a .uproject is added")
    args = parser.parse_args()
    if Path.cwd().resolve() != Path(git("rev-parse", "--show-toplevel").decode("utf-8").strip()).resolve():
        parser.error("run from the repository root")

    paths = set(nul_fields(git("ls-files", "-z")))
    required = {".gitignore", ".gitattributes", ".editorconfig", "README.md", "AGENTS.md", "Docs/Architecture.md"}
    errors = [f"Missing indexed file: {path}" for path in sorted(required - paths)]
    check_project(args.project, paths, args.bootstrap, errors)
    check_ignore_and_lfs(args.project, paths, errors)
    check_text(paths, errors)
    if errors:
        parser.exit(1, "\n".join(errors) + "\n")
    scope = "bootstrap only (.uproject and UE build unavailable)" if args.bootstrap else "project metadata (UE build not executed)"
    print(f"Repository checks passed: {scope}.")


if __name__ == "__main__":
    main()
