#pragma once

#include <cstdint>

namespace wp {
namespace games {
namespace db {

/* 6×6 boxes → 7×7 dots. Fits 480px with a compact dock. */
constexpr int kBox = 6;
constexpr int kDots = kBox + 1;
constexpr int kHEdges = kDots * kBox; /* row-major: r * kBox + c */
constexpr int kVEdges = kBox * kDots; /* row-major: r * kDots + c */
constexpr int kEdges = kHEdges + kVEdges;

constexpr int8_t kNone = 0;
constexpr int8_t kP1 = 1; /* challenger */
constexpr int8_t kP2 = 2; /* acceptor */

struct State {
  int8_t h[kHEdges] = {}; /* owner or 0 */
  int8_t v[kVEdges] = {};
  int8_t box[kBox][kBox] = {}; /* owner or 0 */
  int8_t score1 = 0, score2 = 0;
};

inline void init(State & s) { s = {}; }

inline int8_t h_owner(const State & s, int r, int c) {
  if (r < 0 || r >= kDots || c < 0 || c >= kBox) return kNone;
  return s.h[r * kBox + c];
}
inline int8_t v_owner(const State & s, int r, int c) {
  if (r < 0 || r >= kBox || c < 0 || c >= kDots) return kNone;
  return s.v[r * kDots + c];
}

inline bool h_taken(const State & s, int r, int c) { return h_owner(s, r, c) != kNone; }
inline bool v_taken(const State & s, int r, int c) { return v_owner(s, r, c) != kNone; }

inline int box_sides(const State & s, int br, int bc) {
  int n = 0;
  if (h_taken(s, br, bc)) ++n;
  if (h_taken(s, br + 1, bc)) ++n;
  if (v_taken(s, br, bc)) ++n;
  if (v_taken(s, br, bc + 1)) ++n;
  return n;
}

/** edge: 0 = horizontal (r,c), 1 = vertical (r,c). Returns boxes claimed (0..2). */
inline int claim(State & s, int is_vert, int r, int c, int8_t player) {
  if (is_vert) {
    if (r < 0 || r >= kBox || c < 0 || c >= kDots) return -1;
    const int i = r * kDots + c;
    if (s.v[i]) return -1;
    s.v[i] = player;
  } else {
    if (r < 0 || r >= kDots || c < 0 || c >= kBox) return -1;
    const int i = r * kBox + c;
    if (s.h[i]) return -1;
    s.h[i] = player;
  }

  int claimed = 0;
  auto try_box = [&](int br, int bc) {
    if (br < 0 || br >= kBox || bc < 0 || bc >= kBox) return;
    if (s.box[br][bc]) return;
    if (box_sides(s, br, bc) == 4) {
      s.box[br][bc] = player;
      if (player == kP1) ++s.score1;
      else ++s.score2;
      ++claimed;
    }
  };

  if (is_vert) {
    try_box(r, c - 1);
    try_box(r, c);
  } else {
    try_box(r - 1, c);
    try_box(r, c);
  }
  return claimed;
}

inline bool over(const State & s) { return (s.score1 + s.score2) >= (kBox * kBox); }

/** Winner player 1/2, or -1 tie. Only valid if over. */
inline int8_t winner(const State & s) {
  if (s.score1 > s.score2) return kP1;
  if (s.score2 > s.score1) return kP2;
  return -1;
}

}  // namespace db
}  // namespace games
}  // namespace wp
