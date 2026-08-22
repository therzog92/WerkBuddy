#pragma once

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include "games/wordle_dict.h"

namespace wp {
namespace games {
namespace wordle {

constexpr int kWordLen = 5;
constexpr int kMaxAttempts = 6;

enum class TileState : uint8_t {
  Empty = 0,
  Tbd,
  Absent,
  Present,
  Correct,
};

struct Attempt {
  char word[kWordLen + 1] = {};
  TileState states[kWordLen] = {};
};

struct State {
  char target[kWordLen + 1] = {};
  Attempt attempts[kMaxAttempts] = {};
  int attempt_count = 0;
  char current_input[kWordLen + 1] = {};
  int current_len = 0;
  bool over = false;
  bool won = false;
  TileState key_states[26] = {};
};

inline const char * target_words() {
  return
    "TWINK" "OTTER" "QUEER" "PRIDE" "SLAYS" "TRADE" "HUNKS" "BUTCH" "DADDY" "FEMME"
    "HUNTY" "POPOF" "VOGUE" "CAMPY" "DIVAS" "DISCO" "GLAMS" "BOLLY" "STRUT" "SHADE"
    "READS" "CLOCK" "SERVE" "POSES" "CROWN" "CHUTE" "SNACK" "HOUSE" "BALLS" "CHAPS"
    "DILFS" "CRUSH" "LUNCH" "SWEET" "HONEY" "LOVER" "BEARS" "CUBBY" "PUPPY" "HEELS"
    "DRAMA" "PEACH" "GIRLS" "ROYAL" "MAGIC" "SIREN" "BABES" "CHEEK" "HOTTY" "BEAUT"
    "PUSSY" "BITCH" "SLAYS" "CUNTY" "TWINX" "JOCKS" "TWIRL" "BODYS" "PUMPS" "SLAYY"
    "POOPY" "LOOPY" "GOOPY" "SNOOP" "DROOP" "DERPY" "FARTS" "TURDS" "BOOBS" "BOOBY" "DUMMY"
    "APPLE" "BANAN" "PEARS" "PLUMS" "WATER" "GRAPE" "LEMON" "MELON" "BERRY" "FRUIT";
}

inline int target_count() {
  static int count = -1;
  if (count < 0) {
    count = (int)(std::strlen(target_words()) / kWordLen);
  }
  return count;
}

inline const char * get_target_word(int index) {
  const int total = target_count();
  if (index < 0 || index >= total) return "PRIDE";
  return target_words() + (index * kWordLen);
}

/* Same dictionary for picking AND guessing — no unfair words. */
inline bool is_valid_word(const char * word) {
  if (!word || std::strlen(word) != kWordLen) return false;
  for (int i = 0; i < kWordLen; ++i) {
    if (!std::isalpha((unsigned char)word[i])) return false;
  }
  return dict_contains(word);
}

inline void evaluate_guess(const char * guess, const char * target, TileState out[kWordLen]) {
  char g[kWordLen + 1];
  char t[kWordLen + 1];
  for (int i = 0; i < kWordLen; ++i) {
    g[i] = (char)std::toupper((unsigned char)guess[i]);
    t[i] = (char)std::toupper((unsigned char)target[i]);
    out[i] = TileState::Absent;
  }
  g[kWordLen] = '\0';
  t[kWordLen] = '\0';

  int target_counts[26] = {};
  for (int i = 0; i < kWordLen; ++i) {
    if (t[i] >= 'A' && t[i] <= 'Z') {
      target_counts[t[i] - 'A']++;
    }
  }

  for (int i = 0; i < kWordLen; ++i) {
    if (g[i] == t[i]) {
      out[i] = TileState::Correct;
      target_counts[g[i] - 'A']--;
    }
  }

  for (int i = 0; i < kWordLen; ++i) {
    if (out[i] != TileState::Correct) {
      if (g[i] >= 'A' && g[i] <= 'Z' && target_counts[g[i] - 'A'] > 0) {
        out[i] = TileState::Present;
        target_counts[g[i] - 'A']--;
      } else {
        out[i] = TileState::Absent;
      }
    }
  }
}

inline void new_game(State & state) {
  state = State{};
  const int n = target_count();
  const int pick = std::rand() % n;
  const char * chosen = get_target_word(pick);
  std::memcpy(state.target, chosen, kWordLen);
  state.target[kWordLen] = '\0';
}

inline bool submit_guess(State & state, const char ** err_msg) {
  if (state.over) return false;
  if (state.current_len < kWordLen) {
    if (err_msg) *err_msg = "Not enough letters";
    return false;
  }
  if (!is_valid_word(state.current_input)) {
    if (err_msg) *err_msg = "Not in word list";
    return false;
  }

  Attempt & att = state.attempts[state.attempt_count];
  std::memcpy(att.word, state.current_input, kWordLen);
  att.word[kWordLen] = '\0';
  evaluate_guess(att.word, state.target, att.states);

  for (int i = 0; i < kWordLen; ++i) {
    const char c = att.word[i];
    if (c >= 'A' && c <= 'Z') {
      const int idx = c - 'A';
      const TileState cur = state.key_states[idx];
      const TileState next = att.states[i];
      if (next == TileState::Correct) {
        state.key_states[idx] = TileState::Correct;
      } else if (next == TileState::Present && cur != TileState::Correct) {
        state.key_states[idx] = TileState::Present;
      } else if (next == TileState::Absent && cur == TileState::Empty) {
        state.key_states[idx] = TileState::Absent;
      }
    }
  }

  state.attempt_count++;
  state.current_len = 0;
  state.current_input[0] = '\0';

  if (std::strcmp(att.word, state.target) == 0) {
    state.over = true;
    state.won = true;
  } else if (state.attempt_count >= kMaxAttempts) {
    state.over = true;
    state.won = false;
  }

  return true;
}

inline void add_letter(State & state, char c) {
  if (state.over || state.current_len >= kWordLen) return;
  if (!std::isalpha((unsigned char)c)) return;
  state.current_input[state.current_len++] = (char)std::toupper((unsigned char)c);
  state.current_input[state.current_len] = '\0';
}

inline void delete_letter(State & state) {
  if (state.over || state.current_len <= 0) return;
  state.current_len--;
  state.current_input[state.current_len] = '\0';
}

}
}
}

