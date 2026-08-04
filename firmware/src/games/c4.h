#pragma once

/* Connect Four rules — ported from web/board-games.js (c4Drop / c4Winner). */

#include <cstdint>

namespace wp {
namespace games {
namespace c4 {

constexpr int kCols = 7;
constexpr int kRows = 6;
constexpr int kColorCount = 6; /* pink gold mint sky violet coral */

/** Board cells: -1 empty, else color index 0..5. */
inline void init(int8_t b[kRows][kCols]) {
  for (int r = 0; r < kRows; ++r)
    for (int c = 0; c < kCols; ++c) b[r][c] = -1;
}

/** Drop into column; returns landing row or -1 if full. */
inline int drop(int8_t b[kRows][kCols], int col, int8_t color) {
  for (int r = kRows - 1; r >= 0; --r) {
    if (b[r][col] < 0) {
      b[r][col] = color;
      return r;
    }
  }
  return -1;
}

/** Returns winning color 0..5, kColorCount for draw, or -1 for none. */
inline int winner(const int8_t b[kRows][kCols]) {
  static const int dirs[4][2] = {{0, 1}, {1, 0}, {1, 1}, {1, -1}};
  for (int r = 0; r < kRows; ++r) {
    for (int c = 0; c < kCols; ++c) {
      const int8_t cell = b[r][c];
      if (cell < 0) continue;
      for (const auto & d : dirs) {
        bool ok = true;
        for (int i = 1; i < 4; ++i) {
          const int rr = r + d[0] * i;
          const int cc = c + d[1] * i;
          if (rr < 0 || rr >= kRows || cc < 0 || cc >= kCols || b[rr][cc] != cell) {
            ok = false;
            break;
          }
        }
        if (ok) return cell;
      }
    }
  }
  for (int r = 0; r < kRows; ++r)
    for (int c = 0; c < kCols; ++c)
      if (b[r][c] < 0) return -1;
  return kColorCount; /* draw */
}

}  // namespace c4
}  // namespace games
}  // namespace wp
