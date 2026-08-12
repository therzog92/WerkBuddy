/* Device emoji picker — curated pack only (see bake_emoji_assets.py). */

#include "ui/emoji_palette.h"

#include "emoji_pack.h"

namespace wp {
namespace ui {
namespace {

const char * const * desk_emojis() {
  static const char * ptrs[64];
  static int n = -1;
  if (n < 0) {
    n = emoji_pack::count();
    if (n > 64) n = 64;
    for (int i = 0; i < n; ++i) ptrs[i] = emoji_pack::at(i);
  }
  return ptrs;
}

int desk_count() {
  const int n = emoji_pack::count();
  return n > 64 ? 64 : n;
}

}  // namespace

const EmojiCategory kEmojiCategories[kEmojiCategoryCount] = {
    {"Desk", "📢", desk_emojis(), desk_count()},
};

const char * emoji_at(int flat_index) { return emoji_pack::at(flat_index); }

int emoji_flat_count() { return emoji_pack::count(); }

}  // namespace ui
}  // namespace wp
