#pragma once

#include <cstdint>

#if defined(__wasm__)
#define PHAIS_IMPORT(name) __attribute__((import_module("env"), import_name(name)))
#else
#define PHAIS_IMPORT(name)
#endif

extern "C" {
PHAIS_IMPORT("get_width")        std::int32_t phais_get_width();
PHAIS_IMPORT("get_height")       std::int32_t phais_get_height();
PHAIS_IMPORT("get_turn")         std::int32_t phais_get_turn();
PHAIS_IMPORT("get_max_turns")    std::int32_t phais_get_max_turns();
PHAIS_IMPORT("get_score")        std::int32_t phais_get_score(std::int32_t team);
PHAIS_IMPORT("get_productivity") std::int32_t phais_get_productivity(std::int32_t x, std::int32_t y);
PHAIS_IMPORT("get_bot_x")        std::int32_t phais_get_bot_x(std::int32_t team, std::int32_t index);
PHAIS_IMPORT("get_bot_y")        std::int32_t phais_get_bot_y(std::int32_t team, std::int32_t index);
PHAIS_IMPORT("get_cows")         std::int32_t phais_get_cows(std::int32_t x, std::int32_t y);
PHAIS_IMPORT("submit_move")      std::int32_t phais_submit_move(std::int32_t botlet, std::int32_t direction,
                                                              std::int32_t drag_index);
PHAIS_IMPORT("get_seed")         std::int64_t phais_get_seed();
PHAIS_IMPORT("yield")            void phais_yield();
}

namespace phais {

// Directions use your team's coordinates: north is +y, east is +x.
enum class Direction : std::int32_t { N = 0, E = 1, S = 2, W = 3 };

inline int get_width() { return phais_get_width(); }
inline int get_height() { return phais_get_height(); }
inline int get_turn() { return phais_get_turn(); }
inline int get_max_turns() { return phais_get_max_turns(); }
inline int get_score(int team) { return phais_get_score(team); }
inline int get_productivity(int x, int y) { return phais_get_productivity(x, y); }
inline int get_bot_x(int team, int index) { return phais_get_bot_x(team, index); }
inline int get_bot_y(int team, int index) { return phais_get_bot_y(team, index); }
inline int get_cows(int x, int y) { return phais_get_cows(x, y); }
// drag_index selects a cow on the starting cell in cow-ID order.
// Any invalid index (including the default -1) means move without dragging.
inline int submit_move(int botlet, Direction direction, int drag_index = -1) {
    return phais_submit_move(botlet, static_cast<std::int32_t>(direction), drag_index);
}
inline long long get_seed() { return phais_get_seed(); }
inline void end_turn() { phais_yield(); }

}
