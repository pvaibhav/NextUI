#!/usr/bin/env python3
"""Check the fork updater package before it is bundled in firmware."""

import json
import struct
import sys
import zipfile


def verify(path):
    versions = set()
    with zipfile.ZipFile(path) as package:
        assert package.testzip() is None, "Updater ZIP checksum failed"
        for platform in ("tg5040", "tg5050", "my355", "h700"):
            prefix = f"Tools/{platform}/Updater.pak/"
            metadata = json.loads(package.read(prefix + "pak.json"))
            assert metadata["repo_url"] == "https://github.com/pvaibhav/nextui-updater-pak"
            assert metadata["release_filename"] == "nextui.updater.pakz"
            versions.add(metadata["version"])

            binary = package.read(prefix + "nextui-updater")
            assert binary[:6] == b"\x7fELF\x02\x01", "Expected a 64-bit little-endian ELF"
            assert struct.unpack_from("<H", binary, 18)[0] == 183, "Expected AArch64"
            for value in ("pvaibhav/NextUI", "pvaibhav/nextui-updater-pak", metadata["version"]):
                assert value.encode() in binary, f"Missing updater identity: {value}"
            for name in ("nextui-updater", "launch.sh"):
                mode = package.getinfo(prefix + name).external_attr >> 16
                assert mode & 0o111, f"Not executable: {prefix + name}"
    assert len(versions) == 1, "Platform updater versions differ"
    print(f"Verified fork updater {versions.pop()} for all four platforms.")


if __name__ == "__main__":
    verify(sys.argv[1])
