from pathlib import Path
import re

Import("env")


_INCLUDE_PATTERN = re.compile(
    r'^[ \t]*#[ \t]*include[ \t]*[<"]WiFiClientSecure\.h[>"][ \t]*(?://.*)?(?:\r?\n|$)',
    re.MULTILINE,
)
_MARKER_FILE = ".globalcontroller_wifi_client_secure_patch_applied"
_SOURCE_SUFFIXES = {".h", ".hpp", ".cpp"}


def source_files(dependency_root: Path):
    for path in dependency_root.rglob("*"):
        if path.is_file() and path.suffix.lower() in _SOURCE_SUFFIXES:
            yield path


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

    marker_path = dependency_root / _MARKER_FILE
    patched_files = []

    for path in source_files(dependency_root):
        content = path.read_text(encoding="utf-8")
        patched_content, replacements = _INCLUDE_PATTERN.subn("", content)
        if replacements == 0:
            continue

        path.write_text(patched_content, encoding="utf-8")
        patched_files.append(path.relative_to(dependency_root).as_posix())

    # Validate only the include directive that causes the ESP32/wolfSSL SHA
    # collision. References to a WiFiClientSecure class or identifier in source
    # code are not themselves conflicting includes and must not fail the build.
    remaining_include_files = []
    for path in source_files(dependency_root):
        content = path.read_text(encoding="utf-8")
        if _INCLUDE_PATTERN.search(content):
            remaining_include_files.append(path.relative_to(dependency_root).as_posix())

    if remaining_include_files:
        raise RuntimeError(
            "AndroidTvRemote still imports WiFiClientSecure in: "
            + ", ".join(remaining_include_files)
        )

    marker_path.write_text("compatible\n", encoding="utf-8")

    if patched_files:
        print(
            "GlobalController Android TV compatibility patch: removed unused "
            f"WiFiClientSecure includes from {', '.join(patched_files)}"
        )
    else:
        print(
            "GlobalController Android TV compatibility patch: dependency already compatible"
        )


patch_android_tv_remote()
