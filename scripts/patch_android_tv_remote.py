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
_PAIRING_MESSAGE_MANAGER_PATH = Path("src/pairing/PairingMessageManager.cpp")
_WOLFSSL_HEADER_PATH = Path("src/wolfssl.h")
_SAFE_REMOTE_CLIENT_SOURCE = Path("scripts/android_tv_remote/RemoteClient.cpp")

_PAIRING_UNPACK_STATEMENT = (
    "        Pairing__PairingMessage *response = "
    "pairing__pairing_message__unpack(NULL, chunks.size() - 1, chunks.data() + 1);\n"
)
_PAIRING_NULL_GUARD = (
    "        if (response == nullptr) {\n"
    "            Serial.println(\"[ERROR]: Failed to decode Android TV pairing response\");\n"
    "            chunks.clear();\n"
    "            return;\n"
    "        }\n"
)
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
_PAIRING_CERTIFICATES = (
    "    WOLFSSL_X509 *server_cert = ssl_get_peer_certificate();\n"
    "    WOLFSSL_X509 *client_cert = ssl_get_certificate();\n"
)
_PAIRING_CERTIFICATE_GUARD = (
    "\n    if (server_cert == nullptr || client_cert == nullptr) {\n"
    "        Serial.println(\"[ERROR]: Pairing certificates are unavailable\");\n"
    "        return false;\n"
    "    }\n"
)
_PAIRING_REQUEST_FIELDS = (
    "    message.pairing_request->service_name = (char *)service_name;\n"
    "    message.pairing_request->client_name = model;\n"
)
_PAIRING_REQUEST_FIXED_FIELDS = (
    "    // Android TV Remote v2 requires this fixed service identifier.\n"
    "    // The caller-provided value is the user-visible client name.\n"
    "    message.pairing_request->service_name = (char *)\"atvremote\";\n"
    "    message.pairing_request->client_name = (char *)service_name;\n"
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


def insert_once(path: Path, statement: str, addition: str, description: str) -> bool:
    content = path.read_text(encoding="utf-8")
    if addition.strip() in content:
        return False
    if statement not in content:
        raise RuntimeError(f"Expected {description} statement was not found")
    path.write_text(content.replace(statement, statement + addition, 1), encoding="utf-8")
    return True


def harden_protocol_decoding(dependency_root: Path):
    pairing_path = dependency_root / _PAIRING_MANAGER_PATH
    remote_path = dependency_root / _REMOTE_MANAGER_PATH
    if not pairing_path.exists() or not remote_path.exists():
        raise RuntimeError("AndroidTvRemote manager source files are missing")

    pairing_guard = insert_once(
        pairing_path,
        _PAIRING_UNPACK_STATEMENT,
        _PAIRING_NULL_GUARD,
        "pairing unpack",
    )
    remote_guard = insert_once(
        remote_path,
        _REMOTE_UNPACK_STATEMENT,
        _REMOTE_NULL_GUARD,
        "remote unpack",
    )
    certificate_guard = insert_once(
        pairing_path,
        _PAIRING_CERTIFICATES,
        _PAIRING_CERTIFICATE_GUARD,
        "pairing certificate",
    )
    return pairing_guard, remote_guard, certificate_guard


def fix_pairing_request_fields(dependency_root: Path) -> bool:
    path = dependency_root / _PAIRING_MESSAGE_MANAGER_PATH
    if not path.exists():
        raise RuntimeError("AndroidTvRemote PairingMessageManager.cpp is missing")

    content = path.read_text(encoding="utf-8")
    if _PAIRING_REQUEST_FIXED_FIELDS.strip() in content:
        return False
    if _PAIRING_REQUEST_FIELDS not in content:
        raise RuntimeError("Expected Android TV pairing request fields were not found")

    path.write_text(
        content.replace(
            _PAIRING_REQUEST_FIELDS,
            _PAIRING_REQUEST_FIXED_FIELDS,
            1,
        ),
        encoding="utf-8",
    )
    return True


def replace_remote_client_transport(dependency_root: Path) -> bool:
    project_root = Path(env.subst("$PROJECT_DIR"))
    safe_source_path = project_root / _SAFE_REMOTE_CLIENT_SOURCE
    target_path = dependency_root / _REMOTE_CLIENT_PATH

    if not safe_source_path.exists():
        raise RuntimeError(
            "GlobalController safe Android TV RemoteClient.cpp source is missing"
        )
    if not target_path.exists():
        raise RuntimeError("Pinned AndroidTvRemote RemoteClient.cpp is missing")

    safe_content = safe_source_path.read_text(encoding="utf-8")
    if target_path.read_text(encoding="utf-8") == safe_content:
        return False

    target_path.write_text(safe_content, encoding="utf-8")
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
    pairing_guard, remote_guard, certificate_guard = harden_protocol_decoding(
        android_tv_root
    )
    fixed_pairing_request = fix_pairing_request_fields(android_tv_root)
    replaced_transport = replace_remote_client_transport(android_tv_root)
    changed_wolfssl = ensure_wolfssl_serial_helper_is_declaration(wolfssl_root)

    (android_tv_root / _MARKER_FILE).write_text("compatible\n", encoding="utf-8")

    changes = []
    if removed_includes:
        changes.append("removed WiFiClientSecure includes")
    if added_arduino:
        changes.append("added Arduino.h for Serial")
    if pairing_guard:
        changes.append("guarded pairing protobuf decoding")
    if remote_guard:
        changes.append("guarded remote protobuf decoding")
    if certificate_guard:
        changes.append("guarded pairing certificates")
    if fixed_pairing_request:
        changes.append("fixed Android TV pairing service and client names")
    if replaced_transport:
        changes.append("installed checked TLS transport")
    if changed_wolfssl:
        changes.append("fixed wolfSSL serial helper linkage")

    print(
        "GlobalController compatibility patch: "
        + ("; ".join(changes) if changes else "dependencies already compatible")
    )


patch_dependencies()
