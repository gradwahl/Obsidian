"""Embed a browser helper ELF and its unique fixed-width URL patch location."""
import sys
from pathlib import Path

MARKER = b"WOLODRIVE_BROWSER_URL_PATCH_V1"
PATCH_SIZE = 96


def embed_helper(source, target):
    data = Path(source).read_bytes()
    offset = data.find(MARKER)
    if offset < 0 or data.find(MARKER, offset + 1) >= 0:
        raise ValueError("Helper must contain exactly one URL patch marker")
    if data[offset:offset + PATCH_SIZE] != MARKER.ljust(PATCH_SIZE, b"\0"):
        raise ValueError("Helper URL slot is not a zero-padded 96-byte array")
    with Path(target).open("w", encoding="ascii", newline="\n") as stream:
        stream.write("/* Generated browser helper ELF. */\n")
        stream.write(f"#define BROWSER_URL_OFFSET {offset}\n#define BROWSER_URL_SIZE {PATCH_SIZE}\n")
        stream.write("static const unsigned char browser_helper_elf[] = {\n")
        for start in range(0, len(data), 24):
            stream.write(",".join(str(b) for b in data[start:start + 24]) + ",\n")
        stream.write("};\n")


if __name__ == "__main__":
    embed_helper(sys.argv[1], sys.argv[2])
