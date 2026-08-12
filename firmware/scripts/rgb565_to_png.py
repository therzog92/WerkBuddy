#!/usr/bin/env python3
"""Convert raw RGB565 (LE) framebuffer to PNG. Stdlib only."""
import struct
import sys
import zlib


def main() -> int:
  if len(sys.argv) != 5:
    print("usage: rgb565_to_png.py W H in.rgb565 out.png", file=sys.stderr)
    return 2
  w, h = int(sys.argv[1]), int(sys.argv[2])
  raw = open(sys.argv[3], "rb").read()
  out = sys.argv[4]
  if len(raw) != w * h * 2:
    print(f"bad size {len(raw)} want {w*h*2}", file=sys.stderr)
    return 1
  rows = []
  for y in range(h):
    row = bytearray()
    o = y * w * 2
    for x in range(w):
      p = raw[o + x * 2] | (raw[o + x * 2 + 1] << 8)
      r = ((p >> 11) & 0x1F) << 3
      g = ((p >> 5) & 0x3F) << 2
      b = (p & 0x1F) << 3
      row += bytes((r, g, b))
    rows.append(b"\x00" + bytes(row))

  def chunk(tag: bytes, data: bytes) -> bytes:
    return struct.pack(">I", len(data)) + tag + data + struct.pack(
        ">I", zlib.crc32(tag + data) & 0xFFFFFFFF
    )

  png = b"\x89PNG\r\n\x1a\n"
  png += chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0))
  png += chunk(b"IDAT", zlib.compress(b"".join(rows), 6))
  png += chunk(b"IEND", b"")
  open(out, "wb").write(png)
  print(out)
  return 0


if __name__ == "__main__":
  raise SystemExit(main())
