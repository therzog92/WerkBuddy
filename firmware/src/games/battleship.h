#pragma once

/* Battleship rules — ported from web/board-games.js (fleet placement / fire). */

#include <cstdint>
#include <cstdlib>

namespace wp {
namespace games {
namespace bs {

constexpr int kGrid = 10;
constexpr int kShipCount = 5;

struct ShipSpec {
  const char * name;
  uint8_t len;
};

inline const ShipSpec * ship_specs() {
  static const ShipSpec specs[kShipCount] = {
      {"Carrier", 5}, {"Battleship", 4}, {"Cruiser", 3}, {"Submarine", 3}, {"Destroyer", 2},
  };
  return specs;
}

struct Ship {
  int8_t x = 0, y = 0;
  uint8_t len = 0;
  bool horiz = true;
  uint8_t hits = 0;
  bool placed = false;
};

/*
 * grid cells: -1 empty, 0..4 ship index, kHitCell = ship cell that was hit.
 * Misses on my board are tracked separately (fleet_miss).
 */
constexpr int8_t kHitCell = 100;

struct Fleet {
  int8_t grid[kGrid][kGrid];
  Ship ships[kShipCount];
};

inline void clear_fleet(Fleet & f) {
  for (int y = 0; y < kGrid; ++y)
    for (int x = 0; x < kGrid; ++x) f.grid[y][x] = -1;
  for (int i = 0; i < kShipCount; ++i) f.ships[i] = Ship{};
}

inline int placed_count(const Fleet & f) {
  int n = 0;
  for (int i = 0; i < kShipCount; ++i)
    if (f.ships[i].placed) ++n;
  return n;
}

/** First unplaced ship index, or kShipCount when all placed. */
inline int next_ship_index(const Fleet & f) {
  for (int i = 0; i < kShipCount; ++i)
    if (!f.ships[i].placed) return i;
  return kShipCount;
}

inline bool can_place(const Fleet & f, int x, int y, int len, bool horiz) {
  for (int i = 0; i < len; ++i) {
    const int xx = horiz ? x + i : x;
    const int yy = horiz ? y : y + i;
    if (xx < 0 || yy < 0 || xx >= kGrid || yy >= kGrid) return false;
    if (f.grid[yy][xx] != -1) return false;
  }
  return true;
}

inline void place_ship(Fleet & f, int ship_idx, int x, int y, bool horiz) {
  const int len = ship_specs()[ship_idx].len;
  for (int i = 0; i < len; ++i) {
    const int xx = horiz ? x + i : x;
    const int yy = horiz ? y : y + i;
    f.grid[yy][xx] = (int8_t)ship_idx;
  }
  f.ships[ship_idx] = {(int8_t)x, (int8_t)y, (uint8_t)len, horiz, 0, true};
}

inline void remove_ship(Fleet & f, int ship_idx) {
  const Ship & s = f.ships[ship_idx];
  if (!s.placed) return;
  for (int i = 0; i < s.len; ++i) {
    const int xx = s.horiz ? s.x + i : s.x;
    const int yy = s.horiz ? s.y : s.y + i;
    if (f.grid[yy][xx] == (int8_t)ship_idx) f.grid[yy][xx] = -1;
  }
  f.ships[ship_idx] = Ship{};
}

inline void random_fleet(Fleet & f) {
  clear_fleet(f);
  for (int s = 0; s < kShipCount; ++s) {
    const int len = ship_specs()[s].len;
    for (int attempt = 0; attempt < 200; ++attempt) {
      const bool horiz = (std::rand() & 1) != 0;
      const int x = std::rand() % kGrid;
      const int y = std::rand() % kGrid;
      if (!can_place(f, x, y, len, horiz)) continue;
      place_ship(f, s, x, y, horiz);
      break;
    }
    if (!f.ships[s].placed) {
      /* dense board fallback: restart */
      random_fleet(f);
      return;
    }
  }
}

struct Placement {
  int8_t x, y;
  bool horiz;
};

/**
 * Valid placements of ship `len` that use (ax,ay) as an endpoint (bow/stern).
 * Mirrors web placementOptionsFromAnchor. Returns count (max 4).
 */
inline int placement_options_from_anchor(const Fleet & f, int ax, int ay, int len,
                                         Placement * out) {
  if (f.grid[ay][ax] != -1) return 0;
  const Placement candidates[4] = {
      {(int8_t)ax, (int8_t)ay, true},                  /* extends right */
      {(int8_t)(ax - len + 1), (int8_t)ay, true},      /* extends left */
      {(int8_t)ax, (int8_t)ay, false},                 /* extends down */
      {(int8_t)ax, (int8_t)(ay - len + 1), false},     /* extends up */
  };
  int n = 0;
  for (const Placement & c : candidates) {
    if (!can_place(f, c.x, c.y, len, c.horiz)) continue;
    bool covers = false;
    for (int i = 0; i < len; ++i) {
      const int xx = c.horiz ? c.x + i : c.x;
      const int yy = c.horiz ? c.y : c.y + i;
      if (xx == ax && yy == ay) covers = true;
    }
    if (!covers) continue;
    bool dup = false;
    for (int i = 0; i < n; ++i) {
      if (out[i].x == c.x && out[i].y == c.y && out[i].horiz == c.horiz) dup = true;
    }
    if (!dup) out[n++] = c;
  }
  return n;
}

/** Ship index at cell, or -1 (hit cells no longer identify their ship). */
inline int ship_at(const Fleet & f, int x, int y) {
  const int8_t v = f.grid[y][x];
  return (v >= 0 && v < kShipCount) ? v : -1;
}

struct FireResult {
  bool hit = false;
  bool sunk = false;
  bool game_over = false;
};

/** Resolve an incoming shot against my fleet (mutates grid + ship hits). */
inline FireResult resolve_fire(Fleet & f, int x, int y) {
  FireResult r;
  const int idx = ship_at(f, x, y);
  if (idx >= 0) {
    r.hit = true;
    f.grid[y][x] = kHitCell;
    Ship & s = f.ships[idx];
    s.hits++;
    if (s.hits >= s.len) r.sunk = true;
    r.game_over = true;
    for (int i = 0; i < kShipCount; ++i) {
      if (f.ships[i].placed && f.ships[i].hits < f.ships[i].len) r.game_over = false;
    }
  }
  return r;
}

}  // namespace bs
}  // namespace games
}  // namespace wp
