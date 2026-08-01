from pathlib import Path

Import("env")


def patch_android_tv_remote() -> None:
    dependency_root = (
        Path(env.subst("$PROJECT_LIBDEPS_DIR"))
        / env.subst("$PIOENV")
        / "AndroidTvRemote"
    )

    if not dependency_root.exists():
        raise RuntimeError(
            "AndroidTvRemote dependency is missing; PlatformIO dependency resolution did not complete"
        )

    include_line = "#include <WiFiClientSecure.h>\n"
    patched_files = []

    for path in dependency_root.rglob("*"):
        if path.suffix not in {".h", ".hpp", ".cpp"} or not path.is_file():
            continue

        content = path.read_text(encoding="utf-8")
        if include_line not in content:
            continue

        path.write_text(
            content.replace(include_line, ""),
            encoding="utf-8",
        )
        patched_files.append(path.relative_to(dependency_root).as_posix())

    if not patched_files:
        raise RuntimeError(
            "Expected AndroidTvRemote WiFiClientSecure includes were not found; review the pinned dependency"
        )

    print(
        "GlobalController Android TV compatibility patch: removed unused "
        f"WiFiClientSecure includes from {', '.join(patched_files)}"
    )


patch_android_tv_remote()
