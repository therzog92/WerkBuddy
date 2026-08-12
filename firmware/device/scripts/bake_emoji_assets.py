#!/usr/bin/env python3
"""Bake a curated Twemoji set to RGB565 C arrays for the device (no runtime PNG decode)."""

from __future__ import annotations

import colorsys
import struct
from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parents[2]  # firmware/
SRC = ROOT / "assets" / "emoji"
OUT = ROOT / "device" / "src" / "emoji_assets_gen.cpp"

# Desk pack — requested staples first, then the fun extras we already liked.
CURATED: list[tuple[str, str]] = [
    # Requested staples
    ("🆘", "1f198"),  # SOS
    ("🤬", "1f92c"),  # angry face with swears
    ("🖕", "1f595"),  # middle finger
    ("☕", "2615"),  # coffee
    ("💅", "1f485"),  # nails (recolored pink at bake)
    ("🥪", "1f96a"),  # sandwich
    ("❤️‍🌈", "rainbow_heart"),  # rainbow heart (custom bake)
    ("🚽", "1f6bd"),  # toilet
    ("😢", "1f622"),  # crying
    ("😂", "1f602"),  # crying laughter
    ("❓", "2753"),  # question mark
    # Existing favorites
    ("👑", "1f451"),
    ("📢", "1f4e2"),
    ("👀", "1f440"),
    ("✨", "2728"),
    ("🎉", "1f389"),
    ("❤️", "2764"),
    ("🔥", "1f525"),
    ("💯", "1f4af"),
    ("👍", "1f44d"),
    ("👎", "1f44e"),
    ("🤣", "1f923"),
    ("😍", "1f60d"),
    ("🥺", "1f97a"),
    ("😭", "1f62d"),
    ("😤", "1f624"),
    ("🫡", "1fae1"),
    ("🤝", "1f91d"),
    ("💪", "1f4aa"),
    ("🙏", "1f64f"),
    ("💀", "1f480"),
    ("😎", "1f60e"),
    ("🤔", "1f914"),
    ("😅", "1f605"),
    ("🙄", "1f644"),
    ("😴", "1f634"),
    ("🥳", "1f973"),
    ("😜", "1f61c"),
    ("🍕", "1f355"),
    ("🍻", "1f37b"),
    ("🎮", "1f3ae"),
    ("⚽", "26bd"),
    ("🐕", "1f415"),
    ("🐱", "1f431"),
    ("🌈", "1f308"),
    ("⭐", "2b50"),
    ("💡", "1f4a1"),
    ("📱", "1f4f1"),
    ("⏰", "23f0"),
    ("💼", "1f4bc"),
    ("🚗", "1f697"),
    ("✅", "2705"),
    ("❗", "2757"),
]


def rgb565(r: int, g: int, b: int) -> int:
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)


def nails_to_pink(im: Image.Image) -> Image.Image:
    """Shift red nail-polish pixels toward hot pink."""
    im = im.copy()
    px = im.load()
    w, h = im.size
    for y in range(h):
        for x in range(w):
            r, g, b, a = px[x, y]
            if a < 16:
                continue
            if r > 110 and r >= g + 25 and r >= b + 20:
                # Keep value, pull hue toward pink/magenta.
                px[x, y] = (
                    min(255, int(r * 0.92 + 40)),
                    min(255, int(g * 0.45 + 90)),
                    min(255, int(b * 0.75 + 140)),
                    a,
                )
    return im


def make_rainbow_heart(size: int) -> Image.Image:
    """Heart silhouette from Twemoji ❤️, fill with a vertical rainbow."""
    base = Image.open(SRC / "2764.png").convert("RGBA").resize((size, size), Image.Resampling.LANCZOS)
    out = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    sp = base.load()
    dp = out.load()
    for y in range(size):
        hue = y / max(1, size - 1)  # 0..1 top→bottom
        rr, gg, bb = colorsys.hsv_to_rgb(hue * 0.95, 0.85, 1.0)
        cr, cg, cb = int(rr * 255), int(gg * 255), int(bb * 255)
        for x in range(size):
            r, g, b, a = sp[x, y]
            if a < 16:
                continue
            # Colored where the heart body is (not near-white highlights only).
            lum = (r + g + b) / 3
            if r > 80 and r >= g and a > 40:
                # Blend rainbow with original shading.
                shade = lum / 255.0
                dp[x, y] = (
                    int(cr * (0.55 + 0.45 * shade)),
                    int(cg * (0.55 + 0.45 * shade)),
                    int(cb * (0.55 + 0.45 * shade)),
                    a,
                )
            else:
                dp[x, y] = (r, g, b, a)
    return out


def to_rgb565a8(im: Image.Image, size: int) -> bytes:
    """LV_COLOR_FORMAT_RGB565A8: RGB565 plane, then A8 plane (keeps Twemoji soft edges)."""
    im = im.convert("RGBA").resize((size, size), Image.Resampling.LANCZOS)
    n = size * size
    rgb = bytearray(n * 2)
    alpha = bytearray(n)
    for y in range(size):
        for x in range(size):
            r, g, b, a = im.getpixel((x, y))
            i = y * size + x
            struct.pack_into("<H", rgb, i * 2, rgb565(r, g, b))
            alpha[i] = a
    return bytes(rgb) + bytes(alpha)


def bake_one(utf8: str, stem: str, size: int) -> bytes:
    if stem == "rainbow_heart":
        return to_rgb565a8(make_rainbow_heart(size), size)

    p = SRC / f"{stem}.png"
    if not p.exists():
        p2 = SRC / f"{stem}-fe0f.png"
        if not p2.exists():
            raise FileNotFoundError(stem)
        p = p2

    im = Image.open(p).convert("RGBA")
    if stem == "1f485":
        im = nails_to_pink(im)
    return to_rgb565a8(im, size)


def main() -> None:
    size = 72
    entries: list[tuple[str, str, bytes]] = []
    missing: list[str] = []
    for utf8, stem in CURATED:
        try:
            blob = bake_one(utf8, stem, size)
        except FileNotFoundError:
            missing.append(stem)
            continue
        entries.append((utf8, stem.replace("-", "_"), blob))

    if missing:
        raise SystemExit(f"Missing Twemoji PNGs: {', '.join(missing)}")

    nbytes = size * size * 3  # RGB565 + A8
    lines: list[str] = [
        "/* AUTO-GENERATED by scripts/bake_emoji_assets.py — do not edit. */",
        '#include "lvgl/lvgl.h"',
        "",
        "#include <cstring>",
        "",
        "namespace wp {",
        "namespace ui {",
        "namespace emoji_pack {",
        "",
        f"constexpr int kSizePx = {size};",
        f"constexpr int kCount = {len(entries)};",
        f"constexpr uint32_t kBytes = {nbytes};",
        "",
    ]

    for utf8, stem, blob in entries:
        lines.append(f"static const uint8_t kMap_{stem}[kBytes] = {{")
        cols = 16
        hexes = [f"0x{b:02x}" for b in blob]
        for i in range(0, len(hexes), cols):
            lines.append("  " + ", ".join(hexes[i : i + cols]) + ",")
        lines.append("};")
        lines.append("")

    lines.append("static const uint8_t * const kMaps[kCount] = {")
    for _, stem, _ in entries:
        lines.append(f"  kMap_{stem},")
    lines.append("};")
    lines.append("")

    lines.append("static const char * const kUtf8[kCount] = {")
    for utf8, _, _ in entries:
        esc = "".join(f"\\x{b:02x}" for b in utf8.encode("utf-8"))
        lines.append(f'  "{esc}",')
    lines.append("};")
    lines.append("")

    lines.append("static lv_image_dsc_t kDscs[kCount];")
    lines.append("static bool g_ready = false;")
    lines.append("")
    lines.append("static void ensure() {")
    lines.append("  if (g_ready) return;")
    lines.append("  for (int i = 0; i < kCount; ++i) {")
    lines.append("    lv_image_dsc_t * d = &kDscs[i];")
    lines.append("    std::memset(d, 0, sizeof(*d));")
    lines.append("    d->header.magic = LV_IMAGE_HEADER_MAGIC;")
    lines.append("    d->header.cf = LV_COLOR_FORMAT_RGB565A8;")
    lines.append("    d->header.w = kSizePx;")
    lines.append("    d->header.h = kSizePx;")
    lines.append("    d->header.stride = kSizePx * 2; /* RGB565 row bytes; A8 plane follows */")
    lines.append("    d->data_size = kBytes;")
    lines.append("    d->data = kMaps[i];")
    lines.append("  }")
    lines.append("  g_ready = true;")
    lines.append("}")
    lines.append("")
    lines.append("static uint32_t utf8_cp(const char * s) {")
    lines.append("  const unsigned char * u = (const unsigned char *)s;")
    lines.append("  if (!u || !u[0]) return 0;")
    lines.append("  if (u[0] < 0x80) return u[0];")
    lines.append("  if ((u[0] & 0xE0) == 0xC0) return ((u[0] & 0x1Fu) << 6) | (u[1] & 0x3Fu);")
    lines.append("  if ((u[0] & 0xF0) == 0xE0)")
    lines.append("    return ((u[0] & 0x0Fu) << 12) | ((u[1] & 0x3Fu) << 6) | (u[2] & 0x3Fu);")
    lines.append("  if ((u[0] & 0xF8) == 0xF0)")
    lines.append(
        "    return ((u[0] & 0x07u) << 18) | ((u[1] & 0x3Fu) << 12) | ((u[2] & 0x3Fu) << 6) | (u[3] & 0x3Fu);"
    )
    lines.append("  return 0;")
    lines.append("}")
    lines.append("")
    lines.append("const lv_image_dsc_t * find_dsc(const char * emoji_utf8) {")
    lines.append("  ensure();")
    lines.append("  if (!emoji_utf8 || !emoji_utf8[0]) return nullptr;")
    lines.append("  const uint32_t want = utf8_cp(emoji_utf8);")
    lines.append("  for (int i = 0; i < kCount; ++i) {")
    lines.append("    if (std::strcmp(emoji_utf8, kUtf8[i]) == 0) return &kDscs[i];")
    lines.append("  }")
    lines.append("  /* First-codepoint fallback — skip ZWJ sequences (e.g. rainbow heart). */")
    lines.append("  for (int i = 0; i < kCount; ++i) {")
    lines.append("    const char * u = kUtf8[i];")
    lines.append("    bool zwj = false;")
    lines.append("    for (const char * p = u; *p; ++p) {")
    lines.append("      if ((unsigned char)*p == 0xE2 && (unsigned char)p[1] == 0x80 &&")
    lines.append("          (unsigned char)p[2] == 0x8D) {")
    lines.append("        zwj = true;")
    lines.append("        break;")
    lines.append("      }")
    lines.append("    }")
    lines.append("    if (zwj) continue;")
    lines.append("    if (want && utf8_cp(u) == want) return &kDscs[i];")
    lines.append("  }")
    lines.append("  return nullptr;")
    lines.append("}")
    lines.append("")
    lines.append("int count() { return kCount; }")
    lines.append("const char * at(int i) {")
    lines.append("  if (i < 0 || i >= kCount) return nullptr;")
    lines.append("  return kUtf8[i];")
    lines.append("}")
    lines.append("")
    lines.append("}  // namespace emoji_pack")
    lines.append("}  // namespace ui")
    lines.append("}  // namespace wp")
    lines.append("")

    OUT.write_text("\n".join(lines), encoding="utf-8")
    kb = len(entries) * nbytes / 1024
    print(f"Wrote {OUT} ({len(entries)} emojis, {kb:.1f} KB RGB565 in flash)")


if __name__ == "__main__":
    main()
