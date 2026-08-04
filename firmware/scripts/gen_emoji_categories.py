# -*- coding: utf-8 -*-
"""Build iPhone-style emoji categories from Unicode emoji-test ∩ Twemoji assets."""
import json
import os
import re

TEST = os.path.join(os.path.dirname(__file__), "..", "sim-out", "emoji-test.txt")
ASSETS = os.path.join(os.path.dirname(__file__), "..", "assets", "emoji")
OUT_H = os.path.join(os.path.dirname(__file__), "..", "src", "ui", "emoji_palette.h")
OUT_CPP = os.path.join(os.path.dirname(__file__), "..", "src", "ui", "emoji_palette.cpp")
OUT_JS = os.path.join(os.path.dirname(__file__), "..", "..", "web", "emoji-palette.js")

files = {os.path.splitext(f)[0] for f in os.listdir(ASSETS) if f.endswith(".png")}


def has_asset(emoji: str) -> bool:
    raw = [ord(c) for c in emoji]
    cands = [
        "-".join(f"{c:x}" for c in raw),
        "-".join(f"{c:x}" for c in raw if c != 0xFE0F),
        f"{raw[0]:x}",
        f"{raw[0]:x}-fe0f",
    ]
    return any(c in files for c in cands)


# iPhone-style top-level categories (skip Component, Flags, and tiny Symbols —
# fold Symbols into Objects).
CAT_META = [
    ("Smileys & Emotion", "Smileys", "😀"),
    ("People & Body", "People", "👋"),
    ("Animals & Nature", "Nature", "🐻"),
    ("Food & Drink", "Food", "🍔"),
    ("Activities", "Activity", "⚽"),
    ("Travel & Places", "Travel", "🚗"),
    ("Objects", "Objects", "💡"),
]
# Unicode group name → category index (Symbols merge into Objects)
group_to_cat = {g: i for i, (g, _, _) in enumerate(CAT_META)}
group_to_cat["Symbols"] = next(i for i, (_, label, _) in enumerate(CAT_META) if label == "Objects")
OBJECTS_IDX = group_to_cat["Objects"]

cats: list[list[str]] = [[] for _ in CAT_META]
symbols_first: list[str] = []
seen: set[str] = set()
cur = None
cur_group = None

with open(TEST, encoding="utf-8") as f:
    for line in f:
        if line.startswith("# group:"):
            cur_group = line.split(":", 1)[1].strip()
            cur = group_to_cat.get(cur_group)
            continue
        if cur is None:
            continue
        if "; fully-qualified" not in line:
            continue
        m = re.match(r"^([0-9A-F ]+?)\s*;\s*fully-qualified\s*#\s*(\S+)", line)
        if not m:
            continue
        cps = [int(x, 16) for x in m.group(1).split()]
        if any(0x1F3FB <= c <= 0x1F3FF for c in cps):
            continue
        emoji = m.group(2)
        if len(emoji.encode("utf-8")) > 7:
            continue
        if "\u200d" in emoji or any(c == 0x200D for c in cps):
            continue
        if not has_asset(emoji):
            continue
        key = emoji.replace("\ufe0f", "")
        if key in seen:
            continue
        seen.add(key)
        if cur_group == "Symbols":
            symbols_first.append(emoji)
        else:
            cats[cur].append(emoji)

# Symbols at the front of Objects (lightbulb category)
cats[OBJECTS_IDX] = symbols_first + cats[OBJECTS_IDX]

for i, (_, _, icon) in enumerate(CAT_META):
    if icon not in cats[i] and has_asset(icon):
        if i == OBJECTS_IDX:
            cats[i].insert(len(symbols_first), icon)
        else:
            cats[i].insert(0, icon)

total = sum(len(c) for c in cats)
print("total", total)
for i, (_, label, icon) in enumerate(CAT_META):
    print(f"  {label}: {len(cats[i])}")

h = f"""#pragma once

namespace wp {{
namespace ui {{

struct EmojiCategory {{
  const char * id;   /* short label */
  const char * icon; /* representative emoji */
  const char * const * emojis;
  int count;
}};

constexpr int kEmojiCategoryCount = {len(CAT_META)};
extern const EmojiCategory kEmojiCategories[kEmojiCategoryCount];

/** Flat index across all categories (for callbacks). */
const char * emoji_at(int flat_index);
int emoji_flat_count();

}}  // namespace ui
}}  // namespace wp
"""

cpp_parts = ['#include "ui/emoji_palette.h"\n\nnamespace wp {\nnamespace ui {\nnamespace {\n']
for i, (_, label, icon) in enumerate(CAT_META):
    arr = cats[i]
    cpp_parts.append(f"const char * const kCat{i}[] = {{\n")
    for j in range(0, len(arr), 12):
        chunk = ", ".join(json.dumps(e, ensure_ascii=False) for e in arr[j : j + 12])
        cpp_parts.append(f"    {chunk},\n")
    cpp_parts.append("};\n")

cpp_parts.append("}  // namespace\n\n")
cpp_parts.append("const EmojiCategory kEmojiCategories[kEmojiCategoryCount] = {\n")
for i, (_, label, icon) in enumerate(CAT_META):
    cpp_parts.append(
        f'    {{"{label}", {json.dumps(icon, ensure_ascii=False)}, kCat{i}, '
        f"(int)(sizeof(kCat{i}) / sizeof(kCat{i}[0]))}},\n"
    )
cpp_parts.append("};\n\n")
cpp_parts.append(
    """const char * emoji_at(int flat_index) {
  int base = 0;
  for (int c = 0; c < kEmojiCategoryCount; ++c) {
    const EmojiCategory & cat = kEmojiCategories[c];
    if (flat_index < base + cat.count) return cat.emojis[flat_index - base];
    base += cat.count;
  }
  return nullptr;
}

int emoji_flat_count() {
  int n = 0;
  for (int c = 0; c < kEmojiCategoryCount; ++c) n += kEmojiCategories[c].count;
  return n;
}

}  // namespace ui
}  // namespace wp
"""
)

with open(OUT_H, "w", encoding="utf-8", newline="\n") as f:
    f.write(h)
with open(OUT_CPP, "w", encoding="utf-8", newline="\n") as f:
    f.write("".join(cpp_parts))

js = ["/** iPhone-style emoji categories (Unicode CLDR groups ∩ Twemoji assets). */\n"]
js.append("export const EMOJI_CATEGORIES = [\n")
for i, (_, label, icon) in enumerate(CAT_META):
    js.append(f"  {{\n    id: {json.dumps(label)},\n    icon: {json.dumps(icon, ensure_ascii=False)},\n    emojis: [\n")
    arr = cats[i]
    for j in range(0, len(arr), 12):
        chunk = ", ".join(json.dumps(e, ensure_ascii=False) for e in arr[j : j + 12])
        js.append(f"      {chunk},\n")
    js.append("    ],\n  },\n")
js.append("];\n\n")
js.append("export const EMOJI_PALETTE = EMOJI_CATEGORIES.flatMap((c) => c.emojis);\n")
with open(OUT_JS, "w", encoding="utf-8", newline="\n") as f:
    f.write("".join(js))

print("wrote", OUT_H)
print("wrote", OUT_CPP)
print("wrote", OUT_JS)
