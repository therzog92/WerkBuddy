#pragma once

namespace wp {
namespace games {
namespace wordle {

/* Returns true if the 5-letter word is in the ~14,800-word dictionary.
   Used for BOTH picking a word for your opponent AND validating guesses. */
bool dict_contains(const char * word);

}  // namespace wordle
}  // namespace games
}  // namespace wp

