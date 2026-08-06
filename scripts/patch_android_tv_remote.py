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
_REMOTE_CLIENT_PATH = Path("src/RemoteClient.cpp")
_REMOTE_MANAGER_PATH = Path("src/remote/RemoteManager.cpp")
_REMOTE_MESSAGE_MANAGER_PATH = Path("src/remote/RemoteMessageManager.cpp")
_PAIRING_MANAGER_PATH = Path("src/pairing/PairingManager.cpp")
_WOLFSSL_HEADER_PATH = Path("src/wolfssl.h")
_SAFE_REMOTE_CLIENT_SOURCE = Path("scripts/android_tv_remote/RemoteClient.cpp")
_SAFE_PAIRING_MANAGER_SOURCE = Path("scripts/android_tv_remote/PairingManager.cpp")

_REMOTE_UNPACK_STATEMENT = (
    "        Remote__RemoteMessage *message = "
    "remote__remote_message__unpack(NULL, chunks.size() - 1, chunks.data() + 1);\n"
)
_REMOTE_NULL_GUARD = (
    "        if (message == nullptr) {\n"
    "            Serial.println(\"[ERROR]: Failed to decode Android TV remote response\");\n"
    "            chunks.clear();\n"
    "            return;\n"
    "        }\n"
)
_OLD_SSL_AVAILABLE = (
    "int ssl_available() {\n"
    "    return client.connected() ? client.available() : 0;\n"
    "}\n"
)
_NEW_SSL_AVAILABLE = (
    "int ssl_available() {\n"
    "    if (ssl == nullptr || !client.connected()) {\n"
    "        return 0;\n"
    "    }\n"
    "\n"
    "    return wolfSSL_pending(ssl) + client.available();\n"
    "}\n"
)


def source_files(dependency_root: Path):
    for path in dependency_root.rglob("*"):
        if path.is_file() and path.suffix.lower() in _SOURCE_SUFFIXES:
            yield path


def remove_wifi_client_secure_includes(dependency_root: Path):
    patched_files = []
    for path in source_files(dependency_root):
        content = path.read_text(encoding="utf-8")
        patched, replacements = _WIFI_CLIENT_SECURE_INCLUDE_PATTERN.subn("", content)
        if replacements:
            path.write_text(patched, encoding="utf-8")
            patched_files.append(path.relative_to(dependency_root).as_posix())

    remaining = []
    for path in source_files(dependency_root):
        if _WIFI_CLIENT_SECURE_INCLUDE_PATTERN.search(
            path.read_text(encoding="utf-8")
        ):
            remaining.append(path.relative_to(dependency_root).as_posix())

    if remaining:
        raise RuntimeError(
            "AndroidTvRemote still imports WiFiClientSecure in: "
            + ", ".join(remaining)
        )

    return patched_files


def ensure_arduino_include(dependency_root: Path) -> bool:
    path = dependency_root / _REMOTE_MESSAGE_MANAGER_PATH
    if not path.exists():
        raise RuntimeError("AndroidTvRemote RemoteMessageManager.cpp is missing")

    content = path.read_text(encoding="utf-8")
    if "Serial" not in content or _ARDUINO_INCLUDE_PATTERN.search(content):
        return False

    path.write_text("#include <Arduino.h>\n" + content, encoding="utf-8")
    return True


def guard_remote_decoding(dependency_root: Path) -> bool:
    path = dependency_root / _REMOTE_MANAGER_PATH
    if not path.exists():
        raise RuntimeError("AndroidTvRemote RemoteManager.cpp is missing")

    content = path.read_text(encoding="utf-8")
    if _REMOTE_NULL_GUARD.strip() in content:
        return False
    if _REMOTE_UNPACK_STATEMENT not in content:
        raise RuntimeError("Expected remote protobuf unpack statement was not found")

    path.write_text(
        content.replace(
            _REMOTE_UNPACK_STATEMENT,
            _REMOTE_UNPACK_STATEMENT + _REMOTE_NULL_GUARD,
            1,
        ),
        encoding="utf-8",
    )
    return True


def replace_project_source(
    dependency_root: Path,
    project_relative_source: Path,
    dependency_relative_target: Path,
    description: str,
) -> bool:
    project_root = Path(env.subst("$PROJECT_DIR"))
    source_path = project_root / project_relative_source
    target_path = dependency_root / dependency_relative_target

    if not source_path.exists():
        raise RuntimeError(f"GlobalController {description} source is missing")
    if not target_path.exists():
        raise RuntimeError(f"Pinned AndroidTvRemote {description} target is missing")

    source_content = source_path.read_text(encoding="utf-8")
    if target_path.read_text(encoding="utf-8") == source_content:
        return False

    target_path.write_text(source_content, encoding="utf-8")
    return True


def expose_wolfssl_pending(dependency_root: Path) -> bool:
    path = dependency_root / _REMOTE_CLIENT_PATH
    content = path.read_text(encoding="utf-8")
    if _NEW_SSL_AVAILABLE.strip() in content:
        return False
    if _OLD_SSL_AVAILABLE not in content:
        raise RuntimeError("Expected ssl_available implementation was not found")

    path.write_text(
        content.replace(_OLD_SSL_AVAILABLE, _NEW_SSL_AVAILABLE, 1),
        encoding="utf-8",
    )
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
    raise RuntimeError("wolfSSL_Arduino_Serial_Print has an unterminated body")


def ensure_wolfssl_serial_helper_is_declaration(wolfssl_root: Path) -> bool:
    path = wolfssl_root / _WOLFSSL_HEADER_PATH
    if not path.exists():
        raise RuntimeError("Arduino-wolfSSL src/wolfssl.h is missing")

    content = path.read_text(encoding="utf-8")
    match = _WOLFSSL_SERIAL_SIGNATURE_PATTERN.search(content)
    if match is None:
        raise RuntimeError("Expected wolfSSL_Arduino_Serial_Print signature was not found")

    suffix_index = match.end()
    while suffix_index < len(content) and content[suffix_index].isspace():
        suffix_index += 1

    declaration = (
        match.group("indent")
        + "int wolfSSL_Arduino_Serial_Print(const char* const s);"
    )

    if suffix_index < len(content) and content[suffix_index] == ";":
        current = content[match.start():suffix_index + 1]
        if current == declaration:
            return False
        path.write_text(
            content[:match.start()] + declaration + content[suffix_index + 1:],
            encoding="utf-8",
        )
        return True

    if suffix_index >= len(content) or content[suffix_index] != "{":
        raise RuntimeError("wolfSSL serial helper is neither declaration nor definition")

    closing = find_closing_brace(content, suffix_index)
    replacement_end = closing + 1
    while replacement_end < len(content) and content[replacement_end] in " \t":
        replacement_end += 1
    if replacement_end < len(content) and content[replacement_end] == ";":
        replacement_end += 1

    path.write_text(
        content[:match.start()] + declaration + content[replacement_end:],
        encoding="utf-8",
    )

    if not _WOLFSSL_SERIAL_DECLARATION_PATTERN.search(
        path.read_text(encoding="utf-8")
    ):
        raise RuntimeError("Failed to replace wolfSSL serial helper")
    return True


def patch_dependencies() -> None:
    libdeps_root = Path(env.subst("$PROJECT_LIBDEPS_DIR")) / env.subst("$PIOENV")
    android_tv_root = libdeps_root / "AndroidTvRemote"
    wolfssl_root = libdeps_root / "Arduino-wolfSSL"

    if not android_tv_root.exists() and not wolfssl_root.exists():
        print("GlobalController compatibility patch: dependencies absent; nothing to patch")
        return
    if not android_tv_root.exists() or not wolfssl_root.exists():
        raise RuntimeError("Android TV or wolfSSL dependency is missing")

    removed_includes = remove_wifi_client_secure_includes(android_tv_root)
    added_arduino = ensure_arduino_include(android_tv_root)
    remote_guard = guard_remote_decoding(android_tv_root)
    replaced_transport = replace_project_source(
        android_tv_root,
        _SAFE_REMOTE_CLIENT_SOURCE,
        _REMOTE_CLIENT_PATH,
        "checked TLS transport",
    )
    exposed_pending = expose_wolfssl_pending(android_tv_root)
    replaced_pairing = replace_project_source(
        android_tv_root,
        _SAFE_PAIRING_MANAGER_SOURCE,
        _PAIRING_MANAGER_PATH,
        "pairing protocol engine",
    )
    changed_wolfssl = ensure_wolfssl_serial_helper_is_declaration(wolfssl_root)

    (android_tv_root / _MARKER_FILE).write_text("compatible\n", encoding="utf-8")

    changes = []
    if removed_includes:
        changes.append("removed WiFiClientSecure includes")
    if added_arduino:
        changes.append("added Arduino.h for Serial")
    if remote_guard:
        changes.append("guarded remote protobuf decoding")
    if replaced_transport:
        changes.append("installed checked TLS transport")
    if exposed_pending:
        changes.append("exposed wolfSSL buffered plaintext")
    if replaced_pairing:
        changes.append("installed complete pairing protocol engine")
    if changed_wolfssl:
        changes.append("fixed wolfSSL serial helper linkage")

    print(
        "GlobalController compatibility patch: "
        + ("; ".join(changes) if changes else "dependencies already compatible")
    )


patch_dependencies()
