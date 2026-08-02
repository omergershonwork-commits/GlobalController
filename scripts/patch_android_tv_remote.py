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
_WOLFSSL_SERIAL_SIGNATURE_PATTERN = re.compile(
    r'^(?P<indent>[ \t]*)(?:(?:static[ \t]+)?inline[ \t]+)?int[ \t]+'
    r'wolfSSL_Arduino_Serial_Print[ \t]*\([^)]*\)[ \t]*',
    re.MULTILINE,
)
_WOLFSSL_SERIAL_DECLARATION_PATTERN = re.compile(
    r'^[ \t]*int[ \t]+wolfSSL_Arduino_Serial_Print[ \t]*'
    r'\([ \t]*const[ \t]+char\s*\*[ \t]*const[ \t]+s[ \t]*\)[ \t]*;[ \t]*$',
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


def find_closing_brace(content: str, opening_brace_index: int) -> int:
    depth = 0
    for index in range(opening_brace_index, len(content)):
        character = content[index]
        if character == "{":
            depth += 1
        elif character == "}":
            depth -= 1
            if depth == 0:
                return index

    raise RuntimeError(
        "wolfSSL_Arduino_Serial_Print has an unterminated function body"
    )


def ensure_wolfssl_serial_helper_is_declaration(wolfssl_root: Path) -> bool:
    path = wolfssl_root / _WOLFSSL_HEADER_PATH
    if not path.exists():
        raise RuntimeError(
            "Arduino-wolfSSL src/wolfssl.h is missing from the pinned dependency"
        )

    content = path.read_text(encoding="utf-8")
    signature_match = _WOLFSSL_SERIAL_SIGNATURE_PATTERN.search(content)
    if signature_match is None:
        raise RuntimeError(
            "Expected wolfSSL_Arduino_Serial_Print signature was not found in "
            "Arduino-wolfSSL src/wolfssl.h"
        )

    suffix_index = signature_match.end()
    while suffix_index < len(content) and content[suffix_index].isspace():
        suffix_index += 1

    canonical_declaration = (
        signature_match.group("indent")
        + "int wolfSSL_Arduino_Serial_Print(const char* const s);"
    )

    if suffix_index < len(content) and content[suffix_index] == ";":
        current_declaration = content[signature_match.start():suffix_index + 1]
        if current_declaration == canonical_declaration:
            return False

        path.write_text(
            content[:signature_match.start()]
            + canonical_declaration
            + content[suffix_index + 1:],
            encoding="utf-8",
        )
        return True

    if suffix_index >= len(content) or content[suffix_index] != "{":
        raise RuntimeError(
            "wolfSSL_Arduino_Serial_Print is neither a declaration nor a function definition"
        )

    closing_brace_index = find_closing_brace(content, suffix_index)
    replacement_end = closing_brace_index + 1
    while replacement_end < len(content) and content[replacement_end] in " \t":
        replacement_end += 1
    if replacement_end < len(content) and content[replacement_end] == ";":
        replacement_end += 1

    path.write_text(
        content[:signature_match.start()]
        + canonical_declaration
        + content[replacement_end:],
        encoding="utf-8",
    )

    verified_content = path.read_text(encoding="utf-8")
    if not _WOLFSSL_SERIAL_DECLARATION_PATTERN.search(verified_content):
        raise RuntimeError(
            "Failed to replace wolfSSL_Arduino_Serial_Print with a declaration"
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
    changed_wolfssl_helper = ensure_wolfssl_serial_helper_is_declaration(wolfssl_root)

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
    if changed_wolfssl_helper:
        changes.append(
            "replaced Arduino-wolfSSL serial helper definition with a declaration"
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
