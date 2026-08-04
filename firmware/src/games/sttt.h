#pragma once

/* Super / Ultimate Tic Tac Toe — 9 mini-boards; next board = last cell index.
 * Wiki: https://en.wikipedia.org/wiki/Ultimate_tic-tac-toe */

#include "games/ttt.h"

#include <cstdint>
#include <cstring>

namespace wp {
namespace games {
namespace sttt {

/** meta[i]: 0 open, 'X'/'O' won, 'D' drawn (full, no winner). next_board: -1 = free. */

inline void init(char boards[9][9], char meta[9], int8_t & next_board) {
  std::memset(boards, 0, 9 * 9);
  std::memset(meta, 0, 9);
  next_board = -1;
}

inline bool board_open(const char meta[9], int b) {
  return b >= 0 && b < 9 && !meta[b];
}

/** True when the player must play in `next_board` (that mini-board is still open). */
inline bool forced(const char meta[9], int8_t next_board) {
  return board_open(meta, next_board);
}

inline bool legal(const char boards[9][9], const char meta[9], int8_t next_board, int board,
                  int cell) {
  if (board < 0 || board >= 9 || cell < 0 || cell >= 9) return false;
  if (meta[board] || boards[board][cell]) return false;
  if (forced(meta, next_board) && board != next_board) return false;
  return true;
}

inline void refresh_meta_board(char boards[9][9], char meta[9], int b) {
  if (meta[b]) return;
  const char w = ttt::winner(boards[b]);
  if (w) meta[b] = w;
  else if (ttt::full(boards[b])) meta[b] = 'D';
}

/** Meta-board winner ('X'/'O') or 0. Drawn mini-boards don't count in a line. */
inline char winner(const char meta[9]) {
  static const int lines[8][3] = {
      {0, 1, 2}, {3, 4, 5}, {6, 7, 8}, {0, 3, 6},
      {1, 4, 7}, {2, 5, 8}, {0, 4, 8}, {2, 4, 6},
  };
  for (const auto & ln : lines) {
    const char a = meta[ln[0]];
    if ((a == 'X' || a == 'O') && a == meta[ln[1]] && a == meta[ln[2]]) return a;
  }
  return 0;
}

inline bool any_open(const char meta[9]) {
  for (int i = 0; i < 9; ++i)
    if (!meta[i]) return true;
  return false;
}

inline bool over(const char meta[9]) { return winner(meta) || !any_open(meta); }

/**
 * Place mark. Returns false if illegal. Updates meta + next_board.
 * next_board becomes `cell` if that mini-board is open, else -1 (free).
 */
inline bool play(char boards[9][9], char meta[9], int8_t & next_board, int board, int cell,
                 char mark) {
  if (!legal(boards, meta, next_board, board, cell)) return false;
  boards[board][cell] = mark;
  refresh_meta_board(boards, meta, board);
  next_board = board_open(meta, cell) ? (int8_t)cell : (int8_t)-1;
  return true;
}

}  // namespace sttt
}  // namespace games
}  // namespace wp
