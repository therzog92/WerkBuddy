#pragma once

/* Tic Tac Toe rules — ported from web/app.js (tttGetWinner). */

namespace wp {
namespace games {
namespace ttt {

/** Board: 9 cells of 0 | 'X' | 'O'. Returns winning mark or 0. */
inline char winner(const char b[9]) {
  static const int lines[8][3] = {
      {0, 1, 2}, {3, 4, 5}, {6, 7, 8}, {0, 3, 6},
      {1, 4, 7}, {2, 5, 8}, {0, 4, 8}, {2, 4, 6},
  };
  for (const auto & ln : lines) {
    if (b[ln[0]] && b[ln[0]] == b[ln[1]] && b[ln[0]] == b[ln[2]]) return b[ln[0]];
  }
  return 0;
}

inline bool full(const char b[9]) {
  for (int i = 0; i < 9; ++i) {
    if (!b[i]) return false;
  }
  return true;
}

}  // namespace ttt
}  // namespace games
}  // namespace wp
