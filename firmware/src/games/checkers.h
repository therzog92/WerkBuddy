#pragma once

/* Checkers rules — ported from web/more-games.js (legalMoves / applyCkMove). */

#include <cstdint>

namespace wp {
namespace games {
namespace ck {

constexpr int kSize = 8;

/* Pieces: 0 empty, 'r'/'b' men, 'R'/'B' kings. Black starts top (rows 0-2). */

struct Move {
  int8_t fx, fy, tx, ty;
  bool jump;
};

inline bool is_red(char p) { return p == 'r' || p == 'R'; }
inline bool is_black(char p) { return p == 'b' || p == 'B'; }
inline bool is_king(char p) { return p == 'R' || p == 'B'; }
inline char side_of(char p) {
  if (is_red(p)) return 'r';
  if (is_black(p)) return 'b';
  return 0;
}

inline void init(char b[kSize][kSize]) {
  for (int y = 0; y < kSize; ++y) {
    for (int x = 0; x < kSize; ++x) {
      b[y][x] = 0;
      if ((x + y) % 2 == 0) continue;
      if (y < 3) b[y][x] = 'b';
      if (y > 4) b[y][x] = 'r';
    }
  }
}

/**
 * All legal moves for `side`. Jumps mandatory: if any jump exists only jumps
 * are returned (web behavior). If from_x >= 0, restrict to that piece.
 * Returns count written to out (max entries).
 */
inline int legal_moves(const char b[kSize][kSize], char side, int from_x, int from_y, Move * out,
                       int max) {
  Move jumps[64];
  Move steps[64];
  int nj = 0, ns = 0;

  for (int y = 0; y < kSize; ++y) {
    for (int x = 0; x < kSize; ++x) {
      if (from_x >= 0 && (from_x != x || from_y != y)) continue;
      const char p = b[y][x];
      if (side_of(p) != side) continue;

      static const int all_dirs[4][2] = {{-1, -1}, {1, -1}, {-1, 1}, {1, 1}};

      /* simple steps */
      for (const auto & d : all_dirs) {
        if (!is_king(p)) {
          const bool forward = side == 'r' ? d[1] < 0 : d[1] > 0;
          if (!forward) continue;
        }
        const int nx = x + d[0];
        const int ny = y + d[1];
        if (nx < 0 || nx >= kSize || ny < 0 || ny >= kSize) continue;
        if ((nx + ny) % 2 == 0) continue;
        if (!b[ny][nx] && ns < 64) {
          steps[ns++] = {(int8_t)x, (int8_t)y, (int8_t)nx, (int8_t)ny, false};
        }
      }

      /* jumps */
      for (const auto & d : all_dirs) {
        if (!is_king(p)) {
          const bool forward = side == 'r' ? d[1] < 0 : d[1] > 0;
          if (!forward) continue;
        }
        const int mx = x + d[0];
        const int my = y + d[1];
        const int lx = x + d[0] * 2;
        const int ly = y + d[1] * 2;
        if (lx < 0 || lx >= kSize || ly < 0 || ly >= kSize) continue;
        if ((lx + ly) % 2 == 0) continue;
        const char mid = b[my][mx];
        if (!mid || side_of(mid) == side) continue;
        if (b[ly][lx]) continue;
        if (nj < 64) jumps[nj++] = {(int8_t)x, (int8_t)y, (int8_t)lx, (int8_t)ly, true};
      }
    }
  }

  const Move * src = nj ? jumps : steps;
  const int n = nj ? nj : ns;
  const int count = n < max ? n : max;
  for (int i = 0; i < count; ++i) out[i] = src[i];
  return count;
}

/** Apply move (with capture + kinging). Returns the resulting piece. */
inline char apply_move(char b[kSize][kSize], const Move & m) {
  const char p = b[m.fy][m.fx];
  b[m.fy][m.fx] = 0;
  char piece = p;
  if (is_red(p) && m.ty == 0) piece = 'R';
  if (is_black(p) && m.ty == kSize - 1) piece = 'B';
  b[m.ty][m.tx] = piece;
  if (m.jump) {
    b[(m.fy + m.ty) / 2][(m.fx + m.tx) / 2] = 0;
  }
  return piece;
}

inline int count_side(const char b[kSize][kSize], char side) {
  int n = 0;
  for (int y = 0; y < kSize; ++y)
    for (int x = 0; x < kSize; ++x)
      if (side_of(b[y][x]) == side) ++n;
  return n;
}

}  // namespace ck
}  // namespace games
}  // namespace wp
