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
_WOLFSSL_SERIAL_DEFINITION_PATTERN = re.compile(
    r'^(?P<indent>[ \t]*)int[ \t]+wolfSSL_Arduino_Serial_Print(?=[ \t]*\()',
    re.MULTILINE,
)
_WOLFSSL_INLINE_SERIAL_DEFINITION_PATTERN = re.compile(
    r'^[ \t]*inline[ \t]+int[ \t]+wolfSSL_Arduino_Serial_Print(?=[ \t]*\()',
    re.MULTILINE,
)
_MARKER_FILE = ".globalcontroller_compatibility_patch_applied"
_SOURCE_SUFFIXES = {".h", ".hpp", ".cpp"}
_REMOTE_MESSAGE_MANAGER_PATH = Path("src/remote/RemoteMessageManager.cpp")
_WOLFSSL_HEADER_PATH = Path("src/wolfssl.h")


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


def ensure_wolfssl_serial_helper_is_inline(wolfssl_root: Path) -> bool:
    path = wolfssl_root / _WOLFSSL_HEADER_PATH
    if not path.exists():
        raise RuntimeError(
            "Arduino-wolfSSL src/wolfssl.h is missing from the pinned dependency"
        )

    content = path.read_text(encoding="utf-8")
    if _WOLFSSL_INLINE_SERIAL_DEFINITION_PATTERN.search(content):
        return False

    patched_content, replacements = _WOLFSSL_SERIAL_DEFINITION_PATTERN.subn(
        r"\g<indent>inline int wolfSSL_Arduino_Serial_Print",
        content,
        count=1,
    )
    if replacements != 1:
        raise RuntimeError(
            "Expected wolfSSL_Arduino_Serial_Print definition was not found in "
            "Arduino-wolfSSL src/wolfssl.h"
        )

    path.write_text(patched_content, encoding="utf-8")

    verified_content = path.read_text(encoding="utf-8")
    if not _WOLFSSL_INLINE_SERIAL_DEFINITION_PATTERN.search(verified_content):
        raise RuntimeError(
            "Failed to make wolfSSL_Arduino_Serial_Print inline"
        )

    return True


def patch_dependencies() -> None:
    libdeps_root = (
        Path(env.subst("$PROJECT_LIBDEPS_DIR"))
        / env.subst("$PIOENV")
    )
    android_tv_root = libdeps_root / "AndroidTvRemote"
    wolfssl_root = libdeps_root / "Arduino-wolfSSL"

    # PlatformIO executes pre-scripts for clean targets too. A clean can run
    # before dependencies exist or after they have already been removed.
    if not android_tv_root.exists() and not wolfssl_root.exists():
        print(
            "GlobalController dependency compatibility patch: dependencies are absent; "
            "nothing to patch"
        )
        return

    if not android_tv_root.exists():
        raise RuntimeError(
            "AndroidTvRemote dependency is missing while Arduino-wolfSSL is present"
        )
    if not wolfssl_root.exists():
        raise RuntimeError(
            "Arduino-wolfSSL dependency is missing while AndroidTvRemote is present"
        )

    patched_include_files = remove_wifi_client_secure_includes(android_tv_root)
    added_arduino_include = ensure_arduino_serial_declaration(android_tv_root)
    made_wolfssl_helper_inline = ensure_wolfssl_serial_helper_is_inline(wolfssl_root)

    marker_path = android_tv_root / _MARKER_FILE
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
    if made_wolfssl_helper_inline:
        changes.append(
            "made Arduino-wolfSSL wolfSSL_Arduino_Serial_Print inline"
        )

    if changes:
        print(
            "GlobalController dependency compatibility patch: "
            + "; ".join(changes)
        )
    else:
        print(
            "GlobalController dependency compatibility patch: dependencies already compatible"
        )


patch_dependencies()
