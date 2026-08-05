#pragma once

#include <cstdint>

namespace wp {
namespace games {
namespace rv {

constexpr int kN = 8;
constexpr int8_t kEmpty = 0;
constexpr int8_t kBlack = 1; /* challenger */
constexpr int8_t kWhite = 2; /* acceptor */

inline void init(int8_t b[kN][kN]) {
  for (int r = 0; r < kN; ++r)
    for (int c = 0; c < kN; ++c) b[r][c] = kEmpty;
  b[3][3] = kWhite;
  b[3][4] = kBlack;
  b[4][3] = kBlack;
  b[4][4] = kWhite;
}

inline bool in_bounds(int r, int c) { return r >= 0 && r < kN && c >= 0 && c < kN; }

inline int would_flip(const int8_t b[kN][kN], int r, int c, int8_t me) {
  if (!in_bounds(r, c) || b[r][c] != kEmpty) return 0;
  const int8_t opp = (me == kBlack) ? kWhite : kBlack;
  int total = 0;
  static const int dr[] = {-1, -1, -1, 0, 0, 1, 1, 1};
  static const int dc[] = {-1, 0, 1, -1, 1, -1, 0, 1};
  for (int d = 0; d < 8; ++d) {
    int rr = r + dr[d], cc = c + dc[d], n = 0;
    while (in_bounds(rr, cc) && b[rr][cc] == opp) {
      ++n;
      rr += dr[d];
      cc += dc[d];
    }
    if (n > 0 && in_bounds(rr, cc) && b[rr][cc] == me) total += n;
  }
  return total;
}

inline bool any_move(const int8_t b[kN][kN], int8_t me) {
  for (int r = 0; r < kN; ++r)
    for (int c = 0; c < kN; ++c)
      if (would_flip(b, r, c, me) > 0) return true;
  return false;
}

inline bool apply(int8_t b[kN][kN], int r, int c, int8_t me) {
  if (would_flip(b, r, c, me) <= 0) return false;
  const int8_t opp = (me == kBlack) ? kWhite : kBlack;
  b[r][c] = me;
  static const int dr[] = {-1, -1, -1, 0, 0, 1, 1, 1};
  static const int dc[] = {-1, 0, 1, -1, 1, -1, 0, 1};
  for (int d = 0; d < 8; ++d) {
    int rr = r + dr[d], cc = c + dc[d], n = 0;
    while (in_bounds(rr, cc) && b[rr][cc] == opp) {
      ++n;
      rr += dr[d];
      cc += dc[d];
    }
    if (n > 0 && in_bounds(rr, cc) && b[rr][cc] == me) {
      int fr = r + dr[d], fc = c + dc[d];
      while (in_bounds(fr, fc) && b[fr][fc] == opp) {
        b[fr][fc] = me;
        fr += dr[d];
        fc += dc[d];
      }
    }
  }
  return true;
}

inline void count_pieces(const int8_t b[kN][kN], int * black, int * white) {
  int bl = 0, wh = 0;
  for (int r = 0; r < kN; ++r)
    for (int c = 0; c < kN; ++c) {
      if (b[r][c] == kBlack) ++bl;
      else if (b[r][c] == kWhite) ++wh;
    }
  if (black) *black = bl;
  if (white) *white = wh;
}

/** Returns winner color, 0 if game continues, -1 if draw (both can't move / full). */
inline int8_t check_over(const int8_t b[kN][kN]) {
  const bool mb = any_move(b, kBlack);
  const bool mw = any_move(b, kWhite);
  if (mb || mw) return 0;
  int bl = 0, wh = 0;
  count_pieces(b, &bl, &wh);
  if (bl > wh) return kBlack;
  if (wh > bl) return kWhite;
  return -1;
}

}  // namespace rv
}  // namespace games
}  // namespace wp
