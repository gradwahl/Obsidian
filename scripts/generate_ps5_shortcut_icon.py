"""Generate the native-style red P dashboard mark as PNG and embedded C bytes."""
from pathlib import Path
import struct
import sys
import zlib


def chunk(kind, data):
    return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data))


def make_icon():
    rows = bytearray()
    for y in range(512):
        rows.append(0)
        for x in range(512):
            color = (12, 12, 14)
            stem = 170 <= x < 220 and 128 <= y < 384
            bars = 220 <= x < 275 and (128 <= y < 178 or 238 <= y < 288)
            outer = ((x - 275) / 90)**2 + ((y - 208) / 80)**2
            inner = ((x - 275) / 40)**2 + ((y - 208) / 30)**2
            bowl = x >= 275 and outer <= 1 and inner >= 1
            if stem or bars or bowl:
                color = (229, 9, 20)
            rows.extend(color)
    return (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", 512, 512, 8, 2, 0, 0, 0))
            + chunk(b"IDAT", zlib.compress(bytes(rows), 9)) + chunk(b"IEND", b""))


if __name__ == "__main__":
    png = make_icon()
    Path(sys.argv[1]).write_bytes(png)
    with Path(sys.argv[2]).open("w", encoding="ascii", newline="\n") as stream:
        stream.write("static const unsigned char shortcut_icon_png[] = {\n")
        for start in range(0, len(png), 24):
            stream.write(",".join(str(b) for b in png[start:start + 24]) + ",\n")
        stream.write("};\n")
