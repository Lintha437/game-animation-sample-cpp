"""Install the audited native traversal gameplay hooks in the imported component.

Run in the native Editor after the earlier traversal migrations. Pass
--verify-only to check an existing installation without compiling or saving it.
After installation, repeat verification in a fresh Editor session.
"""

from hashlib import sha256
import json
from pathlib import Path
import shutil
import sys

import unreal


ASSET = "/Game/Blueprints/AC_TraversalLogic"
PARENT = "/Script/GameAnimationSampleCpp.SampleTraversalComponent"
GRAPHS = {"TryTraversalAction", "SetWarpTargets", "EventGraph"}
FUNCTIONS = (
    "TryTraversalAction", "SetWarpTargets", "PerformTraversalAction",
    "PerformTraversalAction_CMC", "PerformTraversalAction_Mover",
    "PerformTraversalAction_Server", "PerformTraversalAction_Clients",
)
NET = 0x00000040
NET_SERVER = 0x00200000
NET_MULTICAST = 0x00004000
RECEIPT_VERSION = 1


def file_hash(path):
    with path.open("rb") as stream:
        digest = sha256()
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def require_clean():
    packages = unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()
    if any(package.get_name() == ASSET for package in packages):
        raise RuntimeError("Reload the dirty traversal component before continuing.")


def function_flags(library, blueprint):
    flags = {name: library.traversal_function_flags(blueprint, name) for name in FUNCTIONS}
    if not all(flags.values()):
        raise RuntimeError("A retained traversal function is absent from the compiled class.")
    if flags["PerformTraversalAction_Server"] & (NET | NET_SERVER) != NET | NET_SERVER:
        raise RuntimeError("The authored server RPC is missing its network flags.")
    if flags["PerformTraversalAction_Clients"] & (NET | NET_MULTICAST) != NET | NET_MULTICAST:
        raise RuntimeError("The authored multicast RPC is missing its network flags.")
    return flags


def graph_signature(library, blueprint):
    signature = library.traversal_graph_signature(blueprint)
    if len(signature) != 40 or any(char not in "0123456789ABCDEF" for char in signature):
        raise RuntimeError("Could not obtain the traversal graph signature.")
    return signature


def verify_installation(library, blueprint, package, backup, receipt_path):
    require_clean()
    if not backup.is_file() or not receipt_path.is_file():
        raise RuntimeError("The migrated traversal component has no complete installation receipt and backup.")
    receipt = json.loads(receipt_path.read_text(encoding="utf-8"))
    if receipt.get("version") != RECEIPT_VERSION or receipt.get("asset") != ASSET:
        raise RuntimeError("Unexpected traversal installation receipt.")
    if not library.validate_traversal_graphs(blueprint):
        raise RuntimeError("The native traversal graph boundaries do not match the installation.")
    if (
        receipt.get("source_package_sha256") != file_hash(backup)
        or receipt.get("installed_package_sha256") != file_hash(package)
        or receipt.get("installed_graph_signature") != graph_signature(library, blueprint)
        or receipt.get("function_flags") != function_flags(library, blueprint)
    ):
        raise RuntimeError("The traversal installation differs from its verified receipt.")
    require_clean()
    unreal.log("Native traversal gameplay installation verified (read-only).")


def migrate(verify_only=False):
    descriptor = Path(unreal.Paths.get_project_file_path()).resolve()
    if descriptor.name != "GameAnimSampleCpp.uproject":
        raise RuntimeError("Run the migration in the native sample project.")
    editor = unreal.BlueprintEditorLibrary
    library = unreal.SampleTraversalBlueprintMigrationLibrary
    require_clean()
    blueprint = unreal.load_asset(ASSET)
    parent = editor.get_blueprint_parent_class(blueprint) if blueprint else None
    if not parent or parent.get_path_name() != PARENT:
        raise RuntimeError("Apply the earlier traversal migrations first.")
    if {graph.get_name() for graph in editor.list_graphs(blueprint)} != GRAPHS:
        raise RuntimeError("Unexpected traversal graph inventory.")
    require_clean()

    package = descriptor.parent / "Content/Blueprints/AC_TraversalLogic.uasset"
    if not package.is_file() or any(package.with_suffix(ext).exists() for ext in (".uexp", ".ubulk", ".uptnl")):
        raise RuntimeError("Expected a single-file traversal Blueprint package.")
    backup = descriptor.parent / "Saved/BlueprintMigration/TraversalNativeGameplay" / package.name
    receipt_path = backup.with_name("installation.json")

    if verify_only or receipt_path.exists() or library.validate_traversal_graphs(blueprint):
        verify_installation(library, blueprint, package, backup, receipt_path)
        return
    if not library.matches_traversal_source(blueprint):
        raise RuntimeError("The traversal source graph differs from the audited topology.")

    original_flags = function_flags(library, blueprint)
    original_signature = graph_signature(library, blueprint)
    original_hash = file_hash(package)
    backup.parent.mkdir(parents=True, exist_ok=True)
    if not backup.exists():
        # Exclusive creation ensures an existing recovery copy is never overwritten.
        with package.open("rb") as source, backup.open("xb") as destination:
            shutil.copyfileobj(source, destination)
    if file_hash(backup) != original_hash or file_hash(package) != original_hash:
        raise RuntimeError(f"Traversal source or backup differs from the preflight: {backup}")
    require_clean()
    if not library.matches_traversal_source(blueprint) or graph_signature(library, blueprint) != original_signature:
        raise RuntimeError("Traversal graph changed during backup. Migration stopped.")

    if not library.migrate_traversal_graphs(blueprint):
        raise RuntimeError(f"Traversal rewiring failed. Discard unsaved changes. Original copy: {backup}")
    if not editor.compile_blueprint(blueprint):
        raise RuntimeError(f"Traversal compilation failed. Discard unsaved changes. Original copy: {backup}")
    if not library.validate_traversal_graphs(blueprint) or function_flags(library, blueprint) != original_flags:
        raise RuntimeError(f"Traversal graph or function verification failed. Discard unsaved changes. Original copy: {backup}")
    installed_signature = graph_signature(library, blueprint)
    if installed_signature == original_signature or file_hash(package) != original_hash:
        raise RuntimeError(f"Unexpected traversal state before saving. Original copy: {backup}")

    assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
    if not assets.save_loaded_asset(blueprint, only_if_is_dirty=False):
        raise RuntimeError(f"Traversal save failed. Original copy: {backup}")
    require_clean()
    installed_hash = file_hash(package)
    if (
        installed_hash == original_hash
        or graph_signature(library, blueprint) != installed_signature
        or not library.validate_traversal_graphs(blueprint)
        or function_flags(library, blueprint) != original_flags
    ):
        raise RuntimeError(f"Saved traversal installation failed verification. Original copy: {backup}")

    receipt = {
        "version": RECEIPT_VERSION,
        "asset": ASSET,
        "source_package_sha256": original_hash,
        "source_graph_signature": original_signature,
        "installed_package_sha256": installed_hash,
        "installed_graph_signature": installed_signature,
        "function_flags": original_flags,
    }
    temporary = receipt_path.with_suffix(".json.pending")
    with temporary.open("x", encoding="utf-8") as stream:
        json.dump(receipt, stream, indent=2, sort_keys=True)
        stream.write("\n")
    temporary.rename(receipt_path)
    verify_installation(library, blueprint, package, backup, receipt_path)


if __name__ == "__main__":
    migrate(verify_only="--verify-only" in sys.argv[1:])
