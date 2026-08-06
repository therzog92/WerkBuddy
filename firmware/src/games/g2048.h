#pragma once

#include <cstdint>
#include <cstdlib>

namespace wp {
namespace games {
namespace g2048 {

constexpr int kSize = 4;
constexpr int kCells = kSize * kSize;

enum class Dir : uint8_t { Up = 0, Down, Left, Right };

/** board[row * 4 + col]; 0 = empty, else tile value (2, 4, 8, …). */
inline void clear(uint16_t board[kCells]) {
  for (int i = 0; i < kCells; ++i) board[i] = 0;
}

inline int empty_count(const uint16_t board[kCells]) {
  int n = 0;
  for (int i = 0; i < kCells; ++i)
    if (!board[i]) ++n;
  return n;
}

/** Spawn 2 (90%) or 4 (10%) in a random empty cell. False if board full. */
inline bool spawn(uint16_t board[kCells]) {
  const int empties = empty_count(board);
  if (empties <= 0) return false;
  int pick = std::rand() % empties;
  for (int i = 0; i < kCells; ++i) {
    if (board[i]) continue;
    if (pick-- == 0) {
      board[i] = (std::rand() % 10 == 0) ? 4 : 2;
      return true;
    }
  }
  return false;
}

inline void new_game(uint16_t board[kCells]) {
  clear(board);
  spawn(board);
  spawn(board);
}

inline bool has_tile(const uint16_t board[kCells], uint16_t v) {
  for (int i = 0; i < kCells; ++i)
    if (board[i] == v) return true;
  return false;
}

/** Slide one line of 4 toward index 0; merges once per tile. Returns score gained. */
inline int slide_line(uint16_t line[kSize]) {
  uint16_t tmp[kSize] = {};
  int w = 0;
  for (int i = 0; i < kSize; ++i)
    if (line[i]) tmp[w++] = line[i];

  int score = 0;
  uint16_t out[kSize] = {};
  int o = 0;
  for (int i = 0; i < w; ++i) {
    if (i + 1 < w && tmp[i] == tmp[i + 1]) {
      const uint16_t m = (uint16_t)(tmp[i] * 2);
      out[o++] = m;
      score += m;
      ++i;
    } else {
      out[o++] = tmp[i];
    }
  }
  for (int i = 0; i < kSize; ++i) line[i] = out[i];
  return score;
}

inline bool lines_equal(const uint16_t a[kSize], const uint16_t b[kSize]) {
  for (int i = 0; i < kSize; ++i)
    if (a[i] != b[i]) return false;
  return true;
}

/**
 * Apply a move. Returns true if the board changed.
 * `score_delta` receives points from merges this move.
 */
inline bool move(uint16_t board[kCells], Dir dir, int & score_delta) {
  score_delta = 0;
  bool changed = false;

  for (int i = 0; i < kSize; ++i) {
    uint16_t line[kSize];
    uint16_t before[kSize];
    for (int j = 0; j < kSize; ++j) {
      int idx = 0;
      switch (dir) {
        case Dir::Left: idx = i * kSize + j; break;
        case Dir::Right: idx = i * kSize + (kSize - 1 - j); break;
        case Dir::Up: idx = j * kSize + i; break;
        case Dir::Down: idx = (kSize - 1 - j) * kSize + i; break;
      }
      line[j] = board[idx];
      before[j] = line[j];
    }
    score_delta += slide_line(line);
    if (!lines_equal(before, line)) changed = true;
    for (int j = 0; j < kSize; ++j) {
      int idx = 0;
      switch (dir) {
        case Dir::Left: idx = i * kSize + j; break;
        case Dir::Right: idx = i * kSize + (kSize - 1 - j); break;
        case Dir::Up: idx = j * kSize + i; break;
        case Dir::Down: idx = (kSize - 1 - j) * kSize + i; break;
      }
      board[idx] = line[j];
    }
  }
  return changed;
}

inline bool can_move(const uint16_t board[kCells]) {
  if (empty_count(board) > 0) return true;
  for (int r = 0; r < kSize; ++r) {
    for (int c = 0; c < kSize; ++c) {
      const uint16_t v = board[r * kSize + c];
      if (c + 1 < kSize && board[r * kSize + c + 1] == v) return true;
      if (r + 1 < kSize && board[(r + 1) * kSize + c] == v) return true;
    }
  }
  return false;
}

}  // namespace g2048
}  // namespace games
}  // namespace wp
