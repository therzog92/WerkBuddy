#include "sim/playthrough.h"

#include "app/active_games.h"
#include "app/app.h"
#include "games/battleship.h"
#include "games/c4.h"
#include "games/checkers.h"
#include "games/dots.h"
#include "games/memory.h"
#include "games/reversi.h"
#include "games/sttt.h"
#include "games/ttt.h"
#include "protocol/messages.h"
#include "sim/screenshot.h"
#include "ui/nav.h"

#include "lvgl/lvgl.h"

#include <cstdio>
#include <cstring>
#include <cstdlib>

namespace wp {
namespace sim {
namespace {

using app::GameKind;

enum class Phase : uint8_t {
  Idle,
  Challenge,
  WaitAccept,
  Play,
  Finish,
  Next,
  Done,
};

constexpr int kMaxKinds = 8;
constexpr uint32_t kAcceptTimeoutMs = 8000;
constexpr uint32_t kStallTimeoutMs = 20000;
constexpr uint32_t kGameHardCapMs = 240000;

GameKind g_queue[kMaxKinds];
int g_queue_n = 0;
int g_queue_i = 0;
Phase g_phase = Phase::Idle;
uint32_t g_phase_t0 = 0;
uint32_t g_game_t0 = 0;
uint32_t g_last_progress = 0;
int g_moves = 0;
char g_reply[512] = {};
char g_log[400] = {};
int g_log_n = 0;
bool g_busy = false;
bool g_reply_ready = false;

void fill_ids(proto::Msg & m, const char * to) {
  const app::Desk & d = app::desk();
  std::snprintf(m.from_id, sizeof(m.from_id), "%s", d.id);
  std::snprintf(m.from_name, sizeof(m.from_name), "%s", d.name);
  std::snprintf(m.to_id, sizeof(m.to_id), "%s", to);
}

const app::Peer * will() {
  const app::Desk & d = app::desk();
  for (int i = 0; i < d.peer_count; ++i)
    if (std::strcmp(d.peers[i].id, "mac-will") == 0) return &d.peers[i];
  return d.peer_count > 0 ? &d.peers[0] : nullptr;
}

const char * kind_code(GameKind kind) {
  switch (kind) {
    case GameKind::Ttt: return "ttt";
    case GameKind::Sttt: return "sttt";
    case GameKind::C4: return "c4";
    case GameKind::Bs: return "bs";
    case GameKind::Ck: return "ck";
    case GameKind::Mem: return "mem";
    case GameKind::Rv: return "rv";
    case GameKind::Db: return "db";
    default: return "game";
  }
}

void log_result(GameKind kind, const char * outcome) {
  char piece[48];
  std::snprintf(piece, sizeof(piece), "%s=%s ", kind_code(kind), outcome);
  const int n = (int)std::strlen(piece);
  if (g_log_n + n < (int)sizeof(g_log)) {
    std::memcpy(g_log + g_log_n, piece, (size_t)n + 1);
    g_log_n += n;
  }
}

void go_kind(GameKind kind) {
  switch (kind) {
    case GameKind::Ttt: ui::go_ttt(); break;
    case GameKind::Sttt: ui::go_sttt(); break;
    case GameKind::C4: ui::go_c4(); break;
    case GameKind::Bs: ui::go_battleship(); break;
    case GameKind::Ck: ui::go_checkers(); break;
    case GameKind::Mem: ui::go_memory(); break;
    case GameKind::Rv: ui::go_reversi(); break;
    case GameKind::Db: ui::go_dots(); break;
    default: break;
  }
}

bool is_over(GameKind kind) {
  switch (kind) {
    case GameKind::Ttt: return app::ttt().over;
    case GameKind::Sttt: return app::sttt().over;
    case GameKind::C4: return app::c4().over;
    case GameKind::Bs: return app::bs().over;
    case GameKind::Ck: return app::ck().over;
    case GameKind::Mem: return app::mem().over;
    case GameKind::Rv: return app::rv().over;
    case GameKind::Db: return app::db().over;
    default: return true;
  }
}

bool is_waiting(GameKind kind) {
  switch (kind) {
    case GameKind::Ttt: return app::ttt().waiting;
    case GameKind::Sttt: return app::sttt().waiting;
    case GameKind::C4: return app::c4().waiting;
    case GameKind::Bs: return app::bs().waiting;
    case GameKind::Ck: return app::ck().waiting;
    case GameKind::Mem: return app::mem().waiting;
    case GameKind::Rv: return app::rv().waiting;
    case GameKind::Db: return app::db().waiting;
    default: return false;
  }
}

bool is_my_turn(GameKind kind) {
  const int idx = app::find_slot(kind, will() ? will()->id : "");
  const app::GameSlot * s = app::slot_at(idx);
  return s && app::is_my_turn(*s);
}

const char * outcome_for(GameKind kind) {
  switch (kind) {
    case GameKind::Ttt: {
      const char w = games::ttt::winner(app::ttt().board);
      if (!w) return "draw";
      return w == app::ttt().mark ? "win" : "loss";
    }
    case GameKind::Sttt: {
      const char w = games::sttt::winner(app::sttt().meta);
      if (!w) return "draw";
      return w == app::sttt().mark ? "win" : "loss";
    }
    case GameKind::C4: {
      const int w = games::c4::winner(app::c4().board);
      if (w < 0) return "draw";
      return w == app::c4().my_color ? "win" : "loss";
    }
    case GameKind::Bs: return app::bs().i_won ? "win" : "loss";
    case GameKind::Ck: {
      const int mine = games::ck::count_side(app::ck().board, app::ck().side);
      const char opp = app::ck().side == 'r' ? 'b' : 'r';
      const int theirs = games::ck::count_side(app::ck().board, opp);
      if (mine == 0) return "loss";
      if (theirs == 0) return "win";
      return mine >= theirs ? "win" : "loss";
    }
    case GameKind::Mem:
      if (app::mem().my_score > app::mem().opp_score) return "win";
      if (app::mem().my_score < app::mem().opp_score) return "loss";
      return "draw";
    case GameKind::Rv: {
      int bl = 0, wh = 0;
      games::rv::count_pieces(app::rv().board, &bl, &wh);
      const int mine = app::rv().my_color == games::rv::kBlack ? bl : wh;
      const int theirs = app::rv().my_color == games::rv::kBlack ? wh : bl;
      if (mine > theirs) return "win";
      if (mine < theirs) return "loss";
      return "draw";
    }
    case GameKind::Db: {
      const int mine = app::db().my_side == games::db::kP1 ? app::db().state.score1
                                                          : app::db().state.score2;
      const int theirs = app::db().my_side == games::db::kP1 ? app::db().state.score2
                                                            : app::db().state.score1;
      if (mine > theirs) return "win";
      if (mine < theirs) return "loss";
      return "draw";
    }
    default: return "done";
  }
}

bool start_challenge(GameKind kind) {
  const app::Peer * p = will();
  if (!p) return false;
  /* Drop any leftover slot for this kind+peer. */
  const int old = app::find_slot(kind, p->id);
  if (old >= 0) {
    app::set_focus(old);
    app::end_focused();
  }
  if (!app::begin_match(kind, p->id)) return false;

  proto::Msg m{};
  fill_ids(m, p->id);

  switch (kind) {
    case GameKind::Ttt:
      app::ttt() = {};
      app::ttt().active = true;
      app::ttt().waiting = true;
      app::ttt().mark = 'X';
      app::ttt().turn = 'X';
      std::snprintf(app::ttt().opp_id, sizeof(app::ttt().opp_id), "%s", p->id);
      std::snprintf(app::ttt().opp_name, sizeof(app::ttt().opp_name), "%s", p->name);
      m.type = proto::MsgType::TttInvite;
      break;
    case GameKind::Sttt:
      app::sttt() = {};
      app::sttt().active = true;
      app::sttt().waiting = true;
      app::sttt().mark = 'X';
      app::sttt().turn = 'X';
      games::sttt::init(app::sttt().boards, app::sttt().meta, app::sttt().next_board);
      std::snprintf(app::sttt().opp_id, sizeof(app::sttt().opp_id), "%s", p->id);
      std::snprintf(app::sttt().opp_name, sizeof(app::sttt().opp_name), "%s", p->name);
      m.type = proto::MsgType::StttInvite;
      break;
    case GameKind::C4:
      app::c4() = {};
      app::c4().active = true;
      app::c4().waiting = true;
      app::c4().my_color = 0;
      app::c4().turn = 0;
      games::c4::init(app::c4().board);
      std::snprintf(app::c4().opp_id, sizeof(app::c4().opp_id), "%s", p->id);
      std::snprintf(app::c4().opp_name, sizeof(app::c4().opp_name), "%s", p->name);
      m.type = proto::MsgType::C4Invite;
      m.color = 0;
      break;
    case GameKind::Bs:
      app::bs() = {};
      app::bs().active = true;
      app::bs().waiting = true;
      app::bs().i_am_first = true;
      std::snprintf(app::bs().opp_id, sizeof(app::bs().opp_id), "%s", p->id);
      std::snprintf(app::bs().opp_name, sizeof(app::bs().opp_name), "%s", p->name);
      m.type = proto::MsgType::BsInvite;
      break;
    case GameKind::Ck:
      app::ck() = {};
      app::ck().active = true;
      app::ck().waiting = true;
      app::ck().side = 'r';
      app::ck().turn = 'r';
      games::ck::init(app::ck().board);
      std::snprintf(app::ck().opp_id, sizeof(app::ck().opp_id), "%s", p->id);
      std::snprintf(app::ck().opp_name, sizeof(app::ck().opp_name), "%s", p->name);
      m.type = proto::MsgType::CkInvite;
      break;
    case GameKind::Mem:
      app::mem() = {};
      app::mem().active = true;
      app::mem().waiting = true;
      app::mem().seed = (uint32_t)std::rand();
      app::mem().my_turn = true;
      games::mem::build_deck(app::mem().seed, app::mem().deck);
      std::snprintf(app::mem().opp_id, sizeof(app::mem().opp_id), "%s", p->id);
      std::snprintf(app::mem().opp_name, sizeof(app::mem().opp_name), "%s", p->name);
      m.type = proto::MsgType::MemInvite;
      m.seed = app::mem().seed;
      break;
    case GameKind::Rv:
      app::rv() = {};
      app::rv().active = true;
      app::rv().waiting = true;
      app::rv().my_color = games::rv::kBlack;
      app::rv().turn = games::rv::kBlack;
      games::rv::init(app::rv().board);
      std::snprintf(app::rv().opp_id, sizeof(app::rv().opp_id), "%s", p->id);
      std::snprintf(app::rv().opp_name, sizeof(app::rv().opp_name), "%s", p->name);
      m.type = proto::MsgType::RvInvite;
      break;
    case GameKind::Db:
      app::db() = {};
      app::db().active = true;
      app::db().waiting = true;
      app::db().my_side = games::db::kP1;
      app::db().turn = games::db::kP1;
      games::db::init(app::db().state);
      std::snprintf(app::db().opp_id, sizeof(app::db().opp_id), "%s", p->id);
      std::snprintf(app::db().opp_name, sizeof(app::db().opp_name), "%s", p->name);
      m.type = proto::MsgType::DbInvite;
      break;
    default: return false;
  }

  app::send(m);
  go_kind(kind);
  return true;
}

bool try_move_ttt() {
  app::TttGame & g = app::ttt();
  if (g.turn != g.mark || g.over || g.waiting) return false;
  for (int i = 0; i < 9; ++i) {
    if (g.board[i]) continue;
    g.board[i] = g.mark;
    proto::Msg m{};
    m.type = proto::MsgType::TttMove;
    fill_ids(m, g.opp_id);
    m.cell = (int8_t)i;
    m.mark = g.mark;
    app::send(m);
    if (games::ttt::winner(g.board) || games::ttt::full(g.board)) {
      g.over = true;
      g.result_dismissed = false;
    } else {
      g.turn = g.mark == 'X' ? 'O' : 'X';
    }
    go_kind(GameKind::Ttt);
    return true;
  }
  return false;
}

bool try_move_sttt() {
  app::StttGame & g = app::sttt();
  if (g.turn != g.mark || g.over || g.waiting) return false;
  for (int b = 0; b < 9; ++b) {
    for (int c = 0; c < 9; ++c) {
      if (!games::sttt::legal(g.boards, g.meta, g.next_board, b, c)) continue;
      games::sttt::play(g.boards, g.meta, g.next_board, b, c, g.mark);
      proto::Msg m{};
      m.type = proto::MsgType::StttMove;
      fill_ids(m, g.opp_id);
      m.col = (int8_t)b;
      m.cell = (int8_t)c;
      m.mark = g.mark;
      app::send(m);
      if (games::sttt::over(g.meta)) {
        g.over = true;
        g.result_dismissed = false;
      } else {
        g.turn = g.mark == 'X' ? 'O' : 'X';
      }
      go_kind(GameKind::Sttt);
      return true;
    }
  }
  return false;
}

bool try_move_c4() {
  app::C4Game & g = app::c4();
  if (g.turn != g.my_color || g.over || g.waiting) return false;
  for (int col = 0; col < games::c4::kCols; ++col) {
    const int row = games::c4::drop(g.board, col, g.my_color);
    if (row < 0) continue;
    g.last_r = (int8_t)row;
    g.last_c = (int8_t)col;
    proto::Msg m{};
    m.type = proto::MsgType::C4Drop;
    fill_ids(m, g.opp_id);
    m.col = (int8_t)col;
    m.color = g.my_color;
    app::send(m);
    if (games::c4::winner(g.board) >= 0) {
      g.over = true;
      g.result_dismissed = false;
    } else {
      g.turn = g.opp_color;
    }
    go_kind(GameKind::C4);
    return true;
  }
  return false;
}

bool try_move_bs() {
  app::BsGame & g = app::bs();
  if (g.waiting || g.over) return false;
  if (g.setup) {
    if (!g.me_ready) {
      if (games::bs::placed_count(g.fleet) < games::bs::kShipCount)
        games::bs::random_fleet(g.fleet);
      g.me_ready = true;
      proto::Msg m{};
      m.type = proto::MsgType::BsReady;
      fill_ids(m, g.opp_id);
      app::send(m);
      if (g.opp_ready) {
        g.setup = false;
        g.my_turn = g.i_am_first;
        g.mode = g.my_turn ? 0 : 1;
      }
      go_kind(GameKind::Bs);
      return true;
    }
    return false; /* wait for opp ready / battle */
  }
  if (!g.my_turn) return false;
  for (int y = 0; y < games::bs::kGrid; ++y) {
    for (int x = 0; x < games::bs::kGrid; ++x) {
      if (g.tracking[y][x]) continue;
      proto::Msg m{};
      m.type = proto::MsgType::BsFire;
      fill_ids(m, g.opp_id);
      m.x = (int8_t)x;
      m.y = (int8_t)y;
      g.my_turn = false;
      app::send(m);
      go_kind(GameKind::Bs);
      return true;
    }
  }
  return false;
}

bool try_move_ck() {
  app::CkGame & g = app::ck();
  if (g.turn != g.side || g.over || g.waiting) return false;
  games::ck::Move moves[64];
  const int from_x = g.must_x >= 0 ? g.must_x : -1;
  const int from_y = g.must_y >= 0 ? g.must_y : -1;
  const int n = games::ck::legal_moves(g.board, g.side, from_x, from_y, moves, 64);
  if (!n) {
    g.over = true;
    g.result_dismissed = false;
    go_kind(GameKind::Ck);
    return true;
  }
  const games::ck::Move & mv = moves[0];
  games::ck::apply_move(g.board, mv);
  proto::Msg m{};
  m.type = proto::MsgType::CkMove;
  fill_ids(m, g.opp_id);
  m.from_x = mv.fx;
  m.from_y = mv.fy;
  m.to_x = mv.tx;
  m.to_y = mv.ty;
  app::send(m);
  if (mv.jump) {
    games::ck::Move more[16];
    const int nm = games::ck::legal_moves(g.board, g.side, mv.tx, mv.ty, more, 16);
    bool has_jump = false;
    for (int i = 0; i < nm; ++i)
      if (more[i].jump) has_jump = true;
    if (has_jump) {
      g.must_x = mv.tx;
      g.must_y = mv.ty;
      g.sel_x = mv.tx;
      g.sel_y = mv.ty;
      go_kind(GameKind::Ck);
      return true;
    }
  }
  g.must_x = g.must_y = g.sel_x = g.sel_y = -1;
  g.turn = g.side == 'r' ? 'b' : 'r';
  games::ck::Move any[64];
  if (games::ck::count_side(g.board, 'r') == 0 || games::ck::count_side(g.board, 'b') == 0 ||
      games::ck::legal_moves(g.board, g.turn, -1, -1, any, 64) == 0) {
    g.over = true;
    g.result_dismissed = false;
  }
  go_kind(GameKind::Ck);
  return true;
}

bool try_move_mem() {
  app::MemGame & g = app::mem();
  if (!g.my_turn || g.over || g.waiting || g.lock) return false;
  int free_cards[16];
  int n = 0;
  for (int i = 0; i < 16; ++i)
    if (!g.matched[i]) free_cards[n++] = i;
  if (n < 2) {
    g.over = true;
    return true;
  }
  /* Prefer a known match if first card chosen; else pick two free. */
  int a = free_cards[0];
  int b = free_cards[1];
  for (int i = 0; i < n; ++i) {
    for (int j = i + 1; j < n; ++j) {
      if (g.deck[free_cards[i]] == g.deck[free_cards[j]]) {
        a = free_cards[i];
        b = free_cards[j];
        goto found;
      }
    }
  }
found:
  g.flip_a = (int8_t)a;
  g.flip_b = (int8_t)b;
  g.lock = true;
  proto::Msg m{};
  m.type = proto::MsgType::MemFlip;
  fill_ids(m, g.opp_id);
  m.card_a = g.flip_a;
  m.card_b = g.flip_b;
  app::send(m);
  const bool match = g.deck[a] == g.deck[b];
  if (match) {
    g.matched[a] = true;
    g.matched[b] = true;
    g.my_score++;
    g.my_turn = true;
  } else {
    g.my_turn = false;
  }
  g.flip_a = g.flip_b = -1;
  g.lock = false;
  bool all = true;
  for (bool mm : g.matched)
    if (!mm) all = false;
  if (all) {
    g.over = true;
    g.result_dismissed = false;
  }
  go_kind(GameKind::Mem);
  return true;
}

bool try_move_rv() {
  app::RvGame & g = app::rv();
  if (g.turn != g.my_color || g.over || g.waiting) return false;
  if (!games::rv::any_move(g.board, g.my_color)) {
    proto::Msg m{};
    m.type = proto::MsgType::RvMove;
    fill_ids(m, g.opp_id);
    m.x = -1;
    m.y = -1;
    app::send(m);
    const int8_t w = games::rv::check_over(g.board);
    const int8_t opp =
        (g.my_color == games::rv::kBlack) ? games::rv::kWhite : games::rv::kBlack;
    if (w != 0) {
      g.over = true;
      g.result_dismissed = false;
    } else if (games::rv::any_move(g.board, opp)) {
      g.turn = opp;
    } else {
      g.over = true;
      g.result_dismissed = false;
    }
    go_kind(GameKind::Rv);
    return true;
  }
  for (int r = 0; r < games::rv::kN; ++r) {
    for (int c = 0; c < games::rv::kN; ++c) {
      if (!games::rv::apply(g.board, r, c, g.my_color)) continue;
      proto::Msg m{};
      m.type = proto::MsgType::RvMove;
      fill_ids(m, g.opp_id);
      m.x = (int8_t)r;
      m.y = (int8_t)c;
      app::send(m);
      const int8_t w = games::rv::check_over(g.board);
      const int8_t opp =
          (g.my_color == games::rv::kBlack) ? games::rv::kWhite : games::rv::kBlack;
      if (w != 0) {
        g.over = true;
        g.result_dismissed = false;
      } else if (games::rv::any_move(g.board, opp)) {
        g.turn = opp;
      } else if (games::rv::any_move(g.board, g.my_color)) {
        g.turn = g.my_color;
      } else {
        g.over = true;
        g.result_dismissed = false;
      }
      go_kind(GameKind::Rv);
      return true;
    }
  }
  return false;
}

bool try_move_db() {
  app::DbGame & g = app::db();
  if (g.turn != g.my_side || g.over || g.waiting) return false;
  for (int r = 0; r < games::db::kDots; ++r) {
    for (int c = 0; c < games::db::kBox; ++c) {
      if (games::db::h_taken(g.state, r, c)) continue;
      const int claimed = games::db::claim(g.state, 0, r, c, g.my_side);
      if (claimed < 0) continue;
      proto::Msg m{};
      m.type = proto::MsgType::DbLine;
      fill_ids(m, g.opp_id);
      m.y = 0;
      m.x = (int8_t)r;
      m.col = (int8_t)c;
      app::send(m);
      if (games::db::over(g.state)) {
        g.over = true;
        g.result_dismissed = false;
      } else if (claimed == 0) {
        g.turn = (g.my_side == games::db::kP1) ? games::db::kP2 : games::db::kP1;
      }
      go_kind(GameKind::Db);
      return true;
    }
  }
  for (int r = 0; r < games::db::kBox; ++r) {
    for (int c = 0; c < games::db::kDots; ++c) {
      if (games::db::v_taken(g.state, r, c)) continue;
      const int claimed = games::db::claim(g.state, 1, r, c, g.my_side);
      if (claimed < 0) continue;
      proto::Msg m{};
      m.type = proto::MsgType::DbLine;
      fill_ids(m, g.opp_id);
      m.y = 1;
      m.x = (int8_t)r;
      m.col = (int8_t)c;
      app::send(m);
      if (games::db::over(g.state)) {
        g.over = true;
        g.result_dismissed = false;
      } else if (claimed == 0) {
        g.turn = (g.my_side == games::db::kP1) ? games::db::kP2 : games::db::kP1;
      }
      go_kind(GameKind::Db);
      return true;
    }
  }
  return false;
}

bool try_one_move(GameKind kind) {
  switch (kind) {
    case GameKind::Ttt: return try_move_ttt();
    case GameKind::Sttt: return try_move_sttt();
    case GameKind::C4: return try_move_c4();
    case GameKind::Bs: return try_move_bs();
    case GameKind::Ck: return try_move_ck();
    case GameKind::Mem: return try_move_mem();
    case GameKind::Rv: return try_move_rv();
    case GameKind::Db: return try_move_db();
    default: return false;
  }
}

GameKind parse_kind(const char * s) {
  if (!std::strcmp(s, "ttt")) return GameKind::Ttt;
  if (!std::strcmp(s, "sttt")) return GameKind::Sttt;
  if (!std::strcmp(s, "c4")) return GameKind::C4;
  if (!std::strcmp(s, "bs") || !std::strcmp(s, "battleship")) return GameKind::Bs;
  if (!std::strcmp(s, "ck") || !std::strcmp(s, "checkers")) return GameKind::Ck;
  if (!std::strcmp(s, "mem") || !std::strcmp(s, "memory")) return GameKind::Mem;
  if (!std::strcmp(s, "rv") || !std::strcmp(s, "reversi")) return GameKind::Rv;
  if (!std::strcmp(s, "db") || !std::strcmp(s, "dots")) return GameKind::Db;
  return GameKind::Count;
}

void finish_done(const char * status) {
  char shot[40];
  std::snprintf(shot, sizeof(shot), "playthrough-%s", status);
  save_named_png(shot);
  std::snprintf(g_reply, sizeof(g_reply), "OK %s %s", status, g_log);
  g_busy = false;
  g_phase = Phase::Done;
  g_reply_ready = true;
  ui::go_hub();
}

}  // namespace

bool playthrough_start(const char * which) {
  if (g_busy) return false;
  g_queue_n = 0;
  g_queue_i = 0;
  g_log[0] = 0;
  g_log_n = 0;
  g_reply_ready = false;
  g_moves = 0;

  if (!which || !which[0] || !std::strcmp(which, "all")) {
    g_queue[g_queue_n++] = GameKind::Ttt;
    g_queue[g_queue_n++] = GameKind::Sttt;
    g_queue[g_queue_n++] = GameKind::C4;
    g_queue[g_queue_n++] = GameKind::Bs;
    g_queue[g_queue_n++] = GameKind::Ck;
    g_queue[g_queue_n++] = GameKind::Mem;
    g_queue[g_queue_n++] = GameKind::Rv;
    g_queue[g_queue_n++] = GameKind::Db;
  } else {
    const GameKind k = parse_kind(which);
    if (k == GameKind::Count) return false;
    g_queue[g_queue_n++] = k;
  }

  g_busy = true;
  g_phase = Phase::Challenge;
  g_phase_t0 = lv_tick_get();
  g_game_t0 = g_phase_t0;
  g_last_progress = g_phase_t0;
  return true;
}

bool playthrough_tick() {
  if (!g_busy || g_reply_ready) return g_reply_ready;
  const uint32_t now = lv_tick_get();
  if (g_queue_i >= g_queue_n) {
    finish_done("complete");
    return true;
  }

  const GameKind kind = g_queue[g_queue_i];

  switch (g_phase) {
    case Phase::Challenge:
      if (!start_challenge(kind)) {
        log_result(kind, "fail-start");
        g_phase = Phase::Next;
        break;
      }
      g_phase = Phase::WaitAccept;
      g_phase_t0 = now;
      g_game_t0 = now;
      g_last_progress = now;
      g_moves = 0;
      break;

    case Phase::WaitAccept:
      if (!is_waiting(kind)) {
        g_phase = Phase::Play;
        g_last_progress = now;
        /* BS: place fleet ASAP once accepted */
        if (kind == GameKind::Bs) try_one_move(kind);
        break;
      }
      if (now - g_phase_t0 > kAcceptTimeoutMs) {
        log_result(kind, "fail-accept");
        app::end_focused();
        g_phase = Phase::Next;
      }
      break;

    case Phase::Play:
      if (is_over(kind)) {
        g_phase = Phase::Finish;
        break;
      }
      if (now - g_game_t0 > kGameHardCapMs) {
        log_result(kind, "timeout");
        char shot[48];
        std::snprintf(shot, sizeof(shot), "pt-%s-timeout", kind_code(kind));
        save_named_png(shot);
        app::end_focused();
        g_phase = Phase::Next;
        break;
      }
      if (is_my_turn(kind) || (kind == GameKind::Bs && app::bs().setup && !app::bs().me_ready)) {
        if (try_one_move(kind)) {
          ++g_moves;
          g_last_progress = now;
        } else if (now - g_last_progress > kStallTimeoutMs) {
          log_result(kind, "stall");
          char shot[48];
          std::snprintf(shot, sizeof(shot), "pt-%s-stall", kind_code(kind));
          save_named_png(shot);
          app::end_focused();
          g_phase = Phase::Next;
        }
      } else if (now - g_last_progress > kStallTimeoutMs) {
        log_result(kind, "stall-wait");
        char shot[48];
        std::snprintf(shot, sizeof(shot), "pt-%s-stallw", kind_code(kind));
        save_named_png(shot);
        app::end_focused();
        g_phase = Phase::Next;
      } else {
        /* Bot thinking — keep UI fresh occasionally */
        if ((now - g_last_progress) % 2000 < 30) go_kind(kind);
      }
      if (is_over(kind)) g_phase = Phase::Finish;
      break;

    case Phase::Finish: {
      const char * out = outcome_for(kind);
      log_result(kind, out);
      char shot[48];
      std::snprintf(shot, sizeof(shot), "pt-%s-%s", kind_code(kind), out);
      save_named_png(shot);
      app::end_focused();
      g_phase = Phase::Next;
      break;
    }

    case Phase::Next:
      ++g_queue_i;
      g_phase = Phase::Challenge;
      g_phase_t0 = now;
      break;

    default:
      break;
  }
  return g_reply_ready;
}

const char * playthrough_reply() { return g_reply; }

bool playthrough_busy() { return g_busy; }

}  // namespace sim
}  // namespace wp
