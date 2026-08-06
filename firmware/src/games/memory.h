#pragma once

/*
 * Memory deck — ported from web/more-games.js (mulberry32 / buildMemoryDeck).
 * Deck built from a shared u32 seed so both desks see identical layouts.
 */

#include <cstdint>

namespace wp {
namespace games {
namespace mem {

constexpr int kCards = 16;
constexpr int kPairs = 8;

/**
 * Pair art is loaded from firmware/assets/memory/*.{png,jpg,jpeg}.
 * UI picks kPairs faces from that folder using the match seed (same seed ⇒
 * same faces on both desks if the folder matches). Drop in more anytime.
 */

/* mulberry32 PRNG — identical constants to the web sim */
struct Rng {
  uint32_t a;
  explicit Rng(uint32_t seed) : a(seed) {}
  /** returns [0, 1) as double via same bit ops as JS */
  double next() {
    a += 0x6d2b79f5u;
    uint32_t t = a;
    t = (uint32_t)((uint64_t)(t ^ (t >> 15)) * (uint64_t)(t | 1u));
    t ^= t + (uint32_t)((uint64_t)(t ^ (t >> 7)) * (uint64_t)(t | 61u));
    return ((t ^ (t >> 14)) >> 0) / 4294967296.0;
  }
};

/** FNV-1a — matches web hashSeed for string seeds (device uses u32 directly). */
inline uint32_t hash_seed(const char * str) {
  uint32_t h = 2166136261u;
  for (const char * p = str; *p; ++p) {
    h ^= (uint8_t)*p;
    h = (uint32_t)((uint64_t)h * 16777619u);
  }
  return h;
}

/** Fisher-Yates with mulberry32 — same order as web shuffleInPlace. */
inline void build_deck(uint32_t seed, uint8_t out[kCards]) {
  for (int i = 0; i < kCards; ++i) out[i] = (uint8_t)(i / 2);
  Rng rng(seed);
  for (int i = kCards - 1; i > 0; --i) {
    const int j = (int)(rng.next() * (i + 1));
    const uint8_t tmp = out[i];
    out[i] = out[j];
    out[j] = tmp;
  }
}

}  // namespace mem
}  // namespace games
}  // namespace wp
