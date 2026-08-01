from pathlib import Path
import re

from SCons.Script import COMMAND_LINE_TARGETS

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
    # PlatformIO also executes pre-scripts for `pio run -t clean`. Cleaning does
    # not compile the dependency and must keep working even when .pio/libdeps
    # has already been removed.
    if "clean" in COMMAND_LINE_TARGETS:
        print("GlobalController Android TV compatibility patch: skipped for clean")
        return

    dependency_root = (
        Path(env.subst("$PROJECT_LIBDEPS_DIR"))
        / env.subst("$PIOENV")
        / "AndroidTvRemote"
    )

    if not dependency_root.exists():
        raise RuntimeError(
            "AndroidTvRemote dependency is missing; PlatformIO dependency resolution did not complete"
        )

    marker_path = dependency_root / _MARKER_FILE
    patched_files = []

    for path in source_files(dependency_root):
        content = path.read_text(encoding="utf-8")
        patched_content, replacements = _INCLUDE_PATTERN.subn("", content)
        if replacements == 0:
            continue

        path.write_text(patched_content, encoding="utf-8")
        patched_files.append(path.relative_to(dependency_root).as_posix())

    if patched_files:
        marker_path.write_text("patched\n", encoding="utf-8")
        print(
            "GlobalController Android TV compatibility patch: removed unused "
            f"WiFiClientSecure includes from {', '.join(patched_files)}"
        )
        return

    if marker_path.exists():
        print(
            "GlobalController Android TV compatibility patch: already applied"
        )
        return

    # Older revisions of this script changed the cached dependency without
    # writing a marker. The pinned dependency may therefore already be in the
    # desired state on a developer machine. Confirm that no source file still
    # references WiFiClientSecure before accepting and marking that state.
    remaining_references = []
    for path in source_files(dependency_root):
        content = path.read_text(encoding="utf-8")
        if "WiFiClientSecure" in content:
            remaining_references.append(path.relative_to(dependency_root).as_posix())

    if remaining_references:
        raise RuntimeError(
            "AndroidTvRemote still references WiFiClientSecure in: "
            + ", ".join(remaining_references)
        )

    marker_path.write_text("already compatible\n", encoding="utf-8")
    print(
        "GlobalController Android TV compatibility patch: includes were already absent; "
        "marked dependency as compatible"
    )


patch_android_tv_remote()
