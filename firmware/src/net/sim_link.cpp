#include "net/link.h"

#include "app/app.h"
#include "games/battleship.h"
#include "games/c4.h"
#include "games/checkers.h"
#include "games/dots.h"
#include "games/memory.h"
#include "games/reversi.h"
#include "games/sttt.h"
#include "games/ttt.h"

#include "lvgl/lvgl.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>

/*
 * PC sim transport: Will and Alex are bots living behind this link. They
 * mirror the web sim's "other desks" — accept invites, play legal moves,
 * shantay pings — so every flow is exercisable without hardware.
 *
 * On the real boards this file is replaced by espnow_link.cpp (see stub).
 */

namespace wp {
namespace net {
namespace {

using proto::Msg;
using proto::MsgType;

void later(uint32_t ms, std::function<void()> fn) {
  auto * f = new std::function<void()>(std::move(fn));
  lv_timer_t * t = lv_timer_create(
      [](lv_timer_t * timer) {
        auto * fn2 = static_cast<std::function<void()> *>(lv_timer_get_user_data(timer));
        (*fn2)();
        delete fn2;
      },
      ms, f);
  lv_timer_set_repeat_count(t, 1);
}

void deliver(const Msg & m) { app::handle_msg(m); }

struct Bot {
  const char * id;
  const char * name;

  /* pager */
  bool call_pending = false;

  /* ttt */
  bool ttt_on = false;
  char ttt_board[9] = {};

  /* super ttt */
  bool sttt_on = false;
  char sttt_boards[9][9] = {};
  char sttt_meta[9] = {};
  int8_t sttt_next = -1;

  /* c4 */
  bool c4_on = false;
  int8_t c4_color = 1;
  int8_t c4_board[games::c4::kRows][games::c4::kCols] = {};

  /* checkers — bot is always the acceptor → black */
  bool ck_on = false;
  char ck_board[games::ck::kSize][games::ck::kSize] = {};

  /* battleship */
  bool bs_on = false;
  bool bs_battle = false;
  games::bs::Fleet bs_fleet;
  int8_t bs_tracking[games::bs::kGrid][games::bs::kGrid] = {};

  /* memory */
  bool mem_on = false;
  uint8_t mem_deck[games::mem::kCards] = {};
  bool mem_matched[games::mem::kCards] = {};

  /* reversi — bot is acceptor → white */
  bool rv_on = false;
  int8_t rv_board[games::rv::kN][games::rv::kN] = {};
  int8_t rv_color = games::rv::kWhite;

  /* dots & boxes — bot is acceptor → P2 */
  bool db_on = false;
  games::db::State db_state;
  int8_t db_side = games::db::kP2;

  bool doodle_replied = false;
};

Bot g_bots[2] = {{"mac-will", "Will"}, {"mac-alex", "Alex"}};

Bot * bot_by_id(const char * id) {
  for (Bot & b : g_bots)
    if (!std::strcmp(b.id, id)) return &b;
  return nullptr;
}

Msg base_msg(const Bot & b, MsgType type) {
  Msg m;
  m.type = type;
  std::snprintf(m.from_id, sizeof(m.from_id), "%s", b.id);
  std::snprintf(m.from_name, sizeof(m.from_name), "%s", b.name);
  std::snprintf(m.to_id, sizeof(m.to_id), "%s", app::desk().id);
  return m;
}

/* ================= TTT bot ================= */

void bot_ttt_move(Bot & b) {
  later(900, [&b]() {
    if (!b.ttt_on) return;
    int empties[9];
    int n = 0;
    for (int i = 0; i < 9; ++i)
      if (!b.ttt_board[i]) empties[n++] = i;
    if (!n) return;
    const int cell = empties[std::rand() % n];
    b.ttt_board[cell] = 'O';
    Msg m = base_msg(b, MsgType::TttMove);
    m.cell = (int8_t)cell;
    m.mark = 'O';
    deliver(m);
    if (games::ttt::winner(b.ttt_board) || games::ttt::full(b.ttt_board)) b.ttt_on = false;
  });
}

/* ================= Super TTT bot ================= */

void bot_sttt_move(Bot & b) {
  later(900, [&b]() {
    if (!b.sttt_on) return;
    struct Move {
      int board, cell;
    };
    Move opts[81];
    int n = 0;
    for (int board = 0; board < 9; ++board) {
      for (int cell = 0; cell < 9; ++cell) {
        if (games::sttt::legal(b.sttt_boards, b.sttt_meta, b.sttt_next, board, cell)) {
          opts[n++] = {board, cell};
        }
      }
    }
    if (!n) {
      b.sttt_on = false;
      return;
    }
    const Move & pick = opts[std::rand() % n];
    games::sttt::play(b.sttt_boards, b.sttt_meta, b.sttt_next, pick.board, pick.cell, 'O');
    Msg m = base_msg(b, MsgType::StttMove);
    m.col = (int8_t)pick.board;
    m.cell = (int8_t)pick.cell;
    m.mark = 'O';
    deliver(m);
    if (games::sttt::over(b.sttt_meta)) b.sttt_on = false;
  });
}

/* ================= C4 bot ================= */

void bot_c4_move(Bot & b) {
  later(900, [&b]() {
    if (!b.c4_on) return;
    int cols[games::c4::kCols];
    int n = 0;
    for (int c = 0; c < games::c4::kCols; ++c)
      if (b.c4_board[0][c] < 0) cols[n++] = c;
    if (!n) return;
    const int col = cols[std::rand() % n];
    games::c4::drop(b.c4_board, col, b.c4_color);
    Msg m = base_msg(b, MsgType::C4Drop);
    m.col = (int8_t)col;
    m.color = b.c4_color;
    deliver(m);
    if (games::c4::winner(b.c4_board) >= 0) b.c4_on = false;
  });
}

/* ================= Checkers bot ================= */

void bot_ck_turn(Bot & b) {
  later(1000, [&b]() {
    if (!b.ck_on) return;
    games::ck::Move moves[64];
    const int n = games::ck::legal_moves(b.ck_board, 'b', -1, -1, moves, 64);
    if (!n) {
      b.ck_on = false; /* stuck — player side detects game over */
      return;
    }
    const games::ck::Move mv = moves[std::rand() % n];
    games::ck::apply_move(b.ck_board, mv);
    Msg m = base_msg(b, MsgType::CkMove);
    m.from_x = mv.fx;
    m.from_y = mv.fy;
    m.to_x = mv.tx;
    m.to_y = mv.ty;
    deliver(m);

    if (mv.jump) {
      games::ck::Move more[16];
      const int nm = games::ck::legal_moves(b.ck_board, 'b', mv.tx, mv.ty, more, 16);
      bool has_jump = false;
      for (int i = 0; i < nm; ++i)
        if (more[i].jump) has_jump = true;
      if (has_jump) {
        bot_ck_turn(b); /* chain the multi-jump */
        return;
      }
    }
    if (games::ck::count_side(b.ck_board, 'r') == 0) b.ck_on = false;
  });
}

/* ================= Battleship bot ================= */

void bot_bs_fire(Bot & b) {
  later(1400, [&b]() {
    if (!b.bs_on || !b.bs_battle) return;
    int xs[100], ys[100], n = 0;
    for (int y = 0; y < games::bs::kGrid; ++y)
      for (int x = 0; x < games::bs::kGrid; ++x)
        if (!b.bs_tracking[y][x]) {
          xs[n] = x;
          ys[n] = y;
          ++n;
        }
    if (!n) return;
    const int pick = std::rand() % n;
    Msg m = base_msg(b, MsgType::BsFire);
    m.x = (int8_t)xs[pick];
    m.y = (int8_t)ys[pick];
    deliver(m);
  });
}

/* ================= Memory bot ================= */

void bot_mem_turn(Bot & b) {
  later(1700, [&b]() {
    if (!b.mem_on) return;
    int free_cards[games::mem::kCards];
    int n = 0;
    for (int i = 0; i < games::mem::kCards; ++i)
      if (!b.mem_matched[i]) free_cards[n++] = i;
    if (n < 2) return;
    const int ai = std::rand() % n;
    int bi = std::rand() % n;
    while (bi == ai) bi = std::rand() % n;
    const int a = free_cards[ai];
    const int c = free_cards[bi];

    Msg m = base_msg(b, MsgType::MemFlip);
    m.card_a = (int8_t)a;
    m.card_b = (int8_t)c;
    deliver(m);

    const bool match = b.mem_deck[a] == b.mem_deck[c];
    if (match) {
      b.mem_matched[a] = true;
      b.mem_matched[c] = true;
      bool all = true;
      for (bool mm : b.mem_matched)
        if (!mm) all = false;
      if (all) {
        b.mem_on = false;
        return;
      }
      bot_mem_turn(b); /* match → bot goes again */
    }
  });
}

/* ================= Doodle bot ================= */

void bot_doodle_reply(Bot & b) {
  later(1800, [&b]() {
    if (b.doodle_replied) return;
    b.doodle_replied = true;
    /* a little smiley: two eye ticks + a smile arc (quantized 0..120) */
    const uint8_t eye1[] = {46, 42, 48, 46};
    const uint8_t eye2[] = {70, 42, 72, 46};
    const uint8_t smile[] = {44, 66, 48, 74, 55, 79, 63, 79, 70, 74, 74, 66};
    struct Stroke {
      const uint8_t * pts;
      uint8_t n;
    } strokes[] = {{eye1, 2}, {eye2, 2}, {smile, 6}};
    uint16_t sid = 900;
    for (const auto & s : strokes) {
      Msg m = base_msg(b, MsgType::DoodleStroke);
      m.stroke_id = sid++;
      m.seq = 0;
      m.last = true;
      m.stroke_color = 2; /* gold */
      m.stroke_w = 2;
      m.n_pts = s.n;
      std::memcpy(m.pts, s.pts, (size_t)s.n * 2);
      deliver(m);
    }
  });
}

/* ================= dispatch ================= */

void bot_receive(Bot & b, const Msg & m) {
  switch (m.type) {
    case MsgType::Call: {
      b.call_pending = true;
      later(2600, [&b, m]() {
        if (!b.call_pending) return;
        b.call_pending = false;
        Msg ack = base_msg(b, MsgType::Ack);
        std::snprintf(ack.for_call_from_id, sizeof(ack.for_call_from_id), "%s", m.from_id);
        deliver(ack);
      });
      return;
    }
    case MsgType::Clear: {
      b.call_pending = false;
      return;
    }

    case MsgType::TttInvite: {
      later(1100, [&b]() {
        b.ttt_on = true;
        std::memset(b.ttt_board, 0, sizeof(b.ttt_board));
        deliver(base_msg(b, MsgType::TttAccept));
      });
      return;
    }
    case MsgType::TttMove: {
      if (!b.ttt_on || m.cell < 0 || m.cell >= 9) return;
      b.ttt_board[m.cell] = m.mark;
      if (games::ttt::winner(b.ttt_board) || games::ttt::full(b.ttt_board)) {
        b.ttt_on = false;
        return;
      }
      bot_ttt_move(b);
      return;
    }
    case MsgType::TttForfeit: {
      b.ttt_on = false;
      return;
    }

    case MsgType::StttInvite: {
      later(1100, [&b]() {
        b.sttt_on = true;
        games::sttt::init(b.sttt_boards, b.sttt_meta, b.sttt_next);
        deliver(base_msg(b, MsgType::StttAccept));
      });
      return;
    }
    case MsgType::StttMove: {
      if (!b.sttt_on) return;
      if (!games::sttt::play(b.sttt_boards, b.sttt_meta, b.sttt_next, m.col, m.cell, m.mark)) return;
      if (games::sttt::over(b.sttt_meta)) {
        b.sttt_on = false;
        return;
      }
      bot_sttt_move(b);
      return;
    }
    case MsgType::StttForfeit: {
      b.sttt_on = false;
      return;
    }

    case MsgType::C4Invite: {
      later(1100, [&b, m]() {
        b.c4_on = true;
        games::c4::init(b.c4_board);
        b.c4_color = m.color == 0 ? 1 : 0; /* web otherDefaultC4Color */
        Msg acc = base_msg(b, MsgType::C4Accept);
        acc.color = b.c4_color;
        deliver(acc);
      });
      return;
    }
    case MsgType::C4Drop: {
      if (!b.c4_on) return;
      games::c4::drop(b.c4_board, m.col, m.color);
      if (games::c4::winner(b.c4_board) >= 0) {
        b.c4_on = false;
        return;
      }
      bot_c4_move(b);
      return;
    }
    case MsgType::C4Forfeit: {
      b.c4_on = false;
      return;
    }

    case MsgType::CkInvite: {
      later(1100, [&b]() {
        b.ck_on = true;
        games::ck::init(b.ck_board);
        deliver(base_msg(b, MsgType::CkAccept));
      });
      return;
    }
    case MsgType::CkMove: {
      if (!b.ck_on) return;
      const bool jump = (m.to_x - m.from_x == 2) || (m.from_x - m.to_x == 2);
      games::ck::Move mv{m.from_x, m.from_y, m.to_x, m.to_y, jump};
      games::ck::apply_move(b.ck_board, mv);
      if (jump) {
        games::ck::Move more[16];
        const int n = games::ck::legal_moves(b.ck_board, 'r', mv.tx, mv.ty, more, 16);
        for (int i = 0; i < n; ++i) {
          if (more[i].jump) return; /* player continues their multi-jump */
        }
      }
      if (games::ck::count_side(b.ck_board, 'b') == 0) {
        b.ck_on = false;
        return;
      }
      bot_ck_turn(b);
      return;
    }
    case MsgType::CkForfeit: {
      b.ck_on = false;
      return;
    }

    case MsgType::BsInvite: {
      later(1100, [&b]() {
        b.bs_on = true;
        b.bs_battle = false;
        games::bs::random_fleet(b.bs_fleet);
        std::memset(b.bs_tracking, 0, sizeof(b.bs_tracking));
        deliver(base_msg(b, MsgType::BsAccept));
        /* "placing ships" pause, then ready */
        later(2400, [&b]() {
          if (!b.bs_on) return;
          deliver(base_msg(b, MsgType::BsReady));
        });
      });
      return;
    }
    case MsgType::BsReady: {
      if (!b.bs_on) return;
      b.bs_battle = true; /* both fleets down once player is ready too */
      return;
    }
    case MsgType::BsFire: {
      if (!b.bs_on) return;
      b.bs_battle = true;
      const auto res = games::bs::resolve_fire(b.bs_fleet, m.x, m.y);
      Msg r = base_msg(b, MsgType::BsResult);
      r.x = m.x;
      r.y = m.y;
      r.hit = res.hit;
      r.sunk = res.sunk;
      r.game_over = res.game_over;
      deliver(r);
      if (res.game_over) {
        b.bs_on = false;
        return;
      }
      bot_bs_fire(b);
      return;
    }
    case MsgType::BsResult: {
      if (!b.bs_on) return;
      b.bs_tracking[m.y][m.x] = m.hit ? 1 : 2;
      if (m.game_over) b.bs_on = false;
      return;
    }
    case MsgType::BsForfeit: {
      b.bs_on = false;
      return;
    }

    case MsgType::MemInvite: {
      later(1100, [&b, m]() {
        b.mem_on = true;
        games::mem::build_deck(m.seed, b.mem_deck);
        std::memset(b.mem_matched, 0, sizeof(b.mem_matched));
        deliver(base_msg(b, MsgType::MemAccept));
      });
      return;
    }
    case MsgType::MemFlip: {
      if (!b.mem_on || m.card_a < 0 || m.card_b < 0) return;
      const bool match = b.mem_deck[m.card_a] == b.mem_deck[m.card_b];
      if (match) {
        b.mem_matched[m.card_a] = true;
        b.mem_matched[m.card_b] = true;
        bool all = true;
        for (bool mm : b.mem_matched)
          if (!mm) all = false;
        if (all) {
          b.mem_on = false;
        }
        return; /* player matched → they go again */
      }
      bot_mem_turn(b);
      return;
    }
    case MsgType::MemForfeit: {
      b.mem_on = false;
      return;
    }

    case MsgType::RvInvite: {
      later(1100, [&b]() {
        b.rv_on = true;
        games::rv::init(b.rv_board);
        b.rv_color = games::rv::kWhite;
        deliver(base_msg(b, MsgType::RvAccept));
      });
      return;
    }
    case MsgType::RvMove: {
      if (!b.rv_on) return;
      const int8_t player = games::rv::kBlack;
      if (m.x < 0 && m.y < 0) {
        /* player passed */
      } else if (!games::rv::apply(b.rv_board, m.x, m.y, player)) {
        return;
      }
      if (games::rv::check_over(b.rv_board) != 0) {
        b.rv_on = false;
        return;
      }
      later(700, [&b]() {
        if (!b.rv_on) return;
        if (!games::rv::any_move(b.rv_board, b.rv_color)) {
          Msg pass = base_msg(b, MsgType::RvMove);
          pass.x = -1;
          pass.y = -1;
          deliver(pass);
          if (games::rv::check_over(b.rv_board) != 0) b.rv_on = false;
          return;
        }
        /* pick a legal move (prefer higher flips) */
        int best_r = -1, best_c = -1, best_n = -1;
        for (int r = 0; r < games::rv::kN; ++r) {
          for (int c = 0; c < games::rv::kN; ++c) {
            const int n = games::rv::would_flip(b.rv_board, r, c, b.rv_color);
            if (n > best_n) {
              best_n = n;
              best_r = r;
              best_c = c;
            }
          }
        }
        if (best_n <= 0) return;
        games::rv::apply(b.rv_board, best_r, best_c, b.rv_color);
        Msg out = base_msg(b, MsgType::RvMove);
        out.x = (int8_t)best_r;
        out.y = (int8_t)best_c;
        deliver(out);
        if (games::rv::check_over(b.rv_board) != 0) b.rv_on = false;
      });
      return;
    }
    case MsgType::RvForfeit: {
      b.rv_on = false;
      return;
    }

    case MsgType::DbInvite: {
      later(1100, [&b]() {
        b.db_on = true;
        games::db::init(b.db_state);
        b.db_side = games::db::kP2;
        deliver(base_msg(b, MsgType::DbAccept));
      });
      return;
    }
    case MsgType::DbLine: {
      if (!b.db_on) return;
      const int claimed = games::db::claim(b.db_state, m.y, m.x, m.col, games::db::kP1);
      if (claimed < 0) return;
      if (games::db::over(b.db_state)) {
        b.db_on = false;
        return;
      }
      if (claimed > 0) return; /* player goes again */
      later(600, [&b]() {
        if (!b.db_on) return;
        /* Keep claiming extra turns until a non-scoring edge or game over. */
        for (;;) {
          bool played = false;
          int last_claimed = 0;
          for (int r = 0; r < games::db::kDots && !played; ++r) {
            for (int c = 0; c < games::db::kBox; ++c) {
              if (games::db::h_taken(b.db_state, r, c)) continue;
              const int cl = games::db::claim(b.db_state, 0, r, c, b.db_side);
              if (cl < 0) continue;
              Msg out = base_msg(b, MsgType::DbLine);
              out.y = 0;
              out.x = (int8_t)r;
              out.col = (int8_t)c;
              deliver(out);
              last_claimed = cl;
              played = true;
              break;
            }
          }
          if (!played) {
            for (int r = 0; r < games::db::kBox && !played; ++r) {
              for (int c = 0; c < games::db::kDots; ++c) {
                if (games::db::v_taken(b.db_state, r, c)) continue;
                const int cl = games::db::claim(b.db_state, 1, r, c, b.db_side);
                if (cl < 0) continue;
                Msg out = base_msg(b, MsgType::DbLine);
                out.y = 1;
                out.x = (int8_t)r;
                out.col = (int8_t)c;
                deliver(out);
                last_claimed = cl;
                played = true;
                break;
              }
            }
          }
          if (!played) return;
          if (games::db::over(b.db_state)) {
            b.db_on = false;
            return;
          }
          if (last_claimed == 0) return;
        }
      });
      return;
    }
    case MsgType::DbForfeit: {
      b.db_on = false;
      return;
    }

    case MsgType::DoodleStroke: {
      bot_doodle_reply(b);
      return;
    }
    case MsgType::DoodleClear: {
      b.doodle_replied = false;
      return;
    }

    default:
      return;
  }
}

}  // namespace

void link_init() {
  /* Sim link needs no setup. ESP-NOW init lands here in Phase 0. */
}

void link_send(const Msg & msg) {
  if (msg.type == MsgType::Discover) {
    int delay = 350;
    for (Bot & b : g_bots) {
      later((uint32_t)delay, [&b]() { deliver(base_msg(b, MsgType::DiscoverReply)); });
      delay += 450;
    }
    return;
  }
  if (msg.type == MsgType::Clear) {
    /* broadcast — every bot with a pending call from us drops it */
    for (Bot & b : g_bots) bot_receive(b, msg);
    return;
  }
  Bot * b = bot_by_id(msg.to_id);
  if (b) bot_receive(*b, msg);
}

}  // namespace net
}  // namespace wp
