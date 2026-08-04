# -*- coding: utf-8 -*-
from pathlib import Path
import re

app = Path(r"c:/Users/Tommy/Projects/WerkPager/web/app.js")
t = app.read_text(encoding="utf-8")
t, n = re.subn(
    r"const MARK_GLYPH = \{ X: \"✖\", O: \"○\" \};\n\nconst EMOJI_PALETTE = \[.*?\];\n\n",
    'const MARK_GLYPH = { X: "✖", O: "○" };\n\n'
    'import { EMOJI_CATEGORIES } from "./emoji-palette.js";\n\n'
    "let emojiPickerCat = 0;\n\n",
    t,
    count=1,
    flags=re.S,
)
print("palette->import", n)
# imports must be top-level - move import next to other imports
t = t.replace(
    'import { installMoreGames } from "./more-games.js";\n',
    'import { installMoreGames } from "./more-games.js";\n'
    'import { EMOJI_CATEGORIES } from "./emoji-palette.js";\n',
)
t = t.replace('import { EMOJI_CATEGORIES } from "./emoji-palette.js";\n\nlet emojiPickerCat = 0;\n\n', "let emojiPickerCat = 0;\n\n")

picker_fn = '''
function openEmojiPicker(index) {
  emojiEditIndex = index;
  emojiPickerCat = 0;
  const tabs = document.getElementById("emojiCats");
  const palette = document.getElementById("emojiPalette");
  const fill = () => {
    palette.innerHTML = "";
    const cat = EMOJI_CATEGORIES[emojiPickerCat];
    if (!cat) return;
    for (const emoji of cat.emojis) {
      const btn = document.createElement("button");
      btn.type = "button";
      btn.textContent = emoji;
      btn.addEventListener("click", () => {
        if (emojiEditIndex < 0) {
          composeEmoji = emoji;
          renderCompose();
          showScreen("compose");
          return;
        }
        desk().emojis[emojiEditIndex] = emoji;
        persistDesk();
        renderSettings();
        showScreen("settings");
        toast("Emoji updated");
      });
      palette.appendChild(btn);
    }
    palette.scrollTop = 0;
  };
  tabs.innerHTML = "";
  EMOJI_CATEGORIES.forEach((cat, i) => {
    const btn = document.createElement("button");
    btn.type = "button";
    btn.className = "emoji-cat";
    btn.textContent = cat.icon;
    btn.title = cat.id;
    btn.setAttribute("aria-pressed", i === emojiPickerCat ? "true" : "false");
    btn.addEventListener("click", () => {
      emojiPickerCat = i;
      for (const el of tabs.querySelectorAll(".emoji-cat")) {
        el.setAttribute("aria-pressed", "false");
      }
      btn.setAttribute("aria-pressed", "true");
      fill();
    });
    tabs.appendChild(btn);
  });
  fill();
  showScreen("emoji-picker");
}
'''

t, n2 = re.subn(
    r"function openEmojiPicker\(index\) \{.*?\n\}\n\nfunction buildOsk",
    picker_fn.strip() + "\n\nfunction buildOsk",
    t,
    count=1,
    flags=re.S,
)
print("replaced openEmojiPicker", n2)
app.write_text(t, encoding="utf-8", newline="\n")
print("ok")
