#pragma once

namespace wp {
namespace ui {

struct EmojiCategory {
  const char * id;   /* short label */
  const char * icon; /* representative emoji */
  const char * const * emojis;
  int count;
};

constexpr int kEmojiCategoryCount = 7;
extern const EmojiCategory kEmojiCategories[kEmojiCategoryCount];

/** Flat index across all categories (for callbacks). */
const char * emoji_at(int flat_index);
int emoji_flat_count();

}  // namespace ui
}  // namespace wp
