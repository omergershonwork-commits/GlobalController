Import("env")

# PlatformIO's ESP32 uploader normally starts a temporary flasher stub in RAM.
# Some ESP32-S3 USB connections lose communication immediately after that stub
# starts. For this project, bypass the stub and use the chip's permanent ROM
# bootloader instead. Also disable compression to reduce transfer complexity.
flags = list(env.get("UPLOADERFLAGS", []))

try:
    write_flash_index = flags.index("write_flash")
except ValueError:
    print("GlobalController upload workaround skipped: esptool write_flash not found")
else:
    if "--no-stub" not in flags:
        flags.insert(write_flash_index, "--no-stub")

    if "-z" in flags:
        flags[flags.index("-z")] = "-u"

    env.Replace(UPLOADERFLAGS=flags)
    print("GlobalController upload workaround: ROM loader, no compression")
