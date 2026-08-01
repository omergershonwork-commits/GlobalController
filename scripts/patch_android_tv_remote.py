from pathlib import Path
import re

Import("env")


_WIFI_CLIENT_SECURE_INCLUDE_PATTERN = re.compile(
    r'^[ \t]*#[ \t]*include[ \t]*[<"]WiFiClientSecure\.h[>"][ \t]*(?://.*)?(?:\r?\n|$)',
    re.MULTILINE,
)
_ARDUINO_INCLUDE_PATTERN = re.compile(
    r'^[ \t]*#[ \t]*include[ \t]*[<"]Arduino\.h[>"][ \t]*(?://.*)?(?:\r?\n|$)',
    re.MULTILINE,
)
_MARKER_FILE = ".globalcontroller_compatibility_patch_applied"
_SOURCE_SUFFIXES = {".h", ".hpp", ".cpp"}
_REMOTE_MESSAGE_MANAGER_PATH = Path("src/remote/RemoteMessageManager.cpp")


def source_files(dependency_root: Path):
    for path in dependency_root.rglob("*"):
        if path.is_file() and path.suffix.lower() in _SOURCE_SUFFIXES:
            yield path


def remove_wifi_client_secure_includes(dependency_root: Path):
    patched_files = []

    for path in source_files(dependency_root):
        content = path.read_text(encoding="utf-8")
        patched_content, replacements = _WIFI_CLIENT_SECURE_INCLUDE_PATTERN.subn("", content)
        if replacements == 0:
            continue

        path.write_text(patched_content, encoding="utf-8")
        patched_files.append(path.relative_to(dependency_root).as_posix())

    remaining_include_files = []
    for path in source_files(dependency_root):
        content = path.read_text(encoding="utf-8")
        if _WIFI_CLIENT_SECURE_INCLUDE_PATTERN.search(content):
            remaining_include_files.append(path.relative_to(dependency_root).as_posix())

    if remaining_include_files:
        raise RuntimeError(
            "AndroidTvRemote still imports WiFiClientSecure in: "
            + ", ".join(remaining_include_files)
        )

    return patched_files


def ensure_arduino_serial_declaration(dependency_root: Path) -> bool:
    path = dependency_root / _REMOTE_MESSAGE_MANAGER_PATH
    if not path.exists():
        raise RuntimeError(
            "AndroidTvRemote RemoteMessageManager.cpp is missing from the pinned dependency"
        )

    content = path.read_text(encoding="utf-8")
    if "Serial" not in content:
        return False

    if _ARDUINO_INCLUDE_PATTERN.search(content):
        return False

    path.write_text("#include <Arduino.h>\n" + content, encoding="utf-8")
    return True


def patch_android_tv_remote() -> None:
    dependency_root = (
        Path(env.subst("$PROJECT_LIBDEPS_DIR"))
        / env.subst("$PIOENV")
        / "AndroidTvRemote"
    )

    # PlatformIO executes pre-scripts for clean targets too. A clean can run
    # before dependencies exist or after they have already been removed.
    if not dependency_root.exists():
        print(
            "GlobalController Android TV compatibility patch: dependency is absent; "
            "nothing to patch"
        )
        return

    patched_include_files = remove_wifi_client_secure_includes(dependency_root)
    added_arduino_include = ensure_arduino_serial_declaration(dependency_root)

    marker_path = dependency_root / _MARKER_FILE
    marker_path.write_text("compatible\n", encoding="utf-8")

    changes = []
    if patched_include_files:
        changes.append(
            "removed unused WiFiClientSecure includes from "
            + ", ".join(patched_include_files)
        )
    if added_arduino_include:
        changes.append(
            "added Arduino.h to src/remote/RemoteMessageManager.cpp for Serial"
        )

    if changes:
        print(
            "GlobalController Android TV compatibility patch: "
            + "; ".join(changes)
        )
    else:
        print(
            "GlobalController Android TV compatibility patch: dependency already compatible"
        )


patch_android_tv_remote()
