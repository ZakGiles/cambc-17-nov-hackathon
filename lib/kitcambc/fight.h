#pragma once
// Fight or flee, whom to hit, where to stand. From cambc's fighter.
#include <algorithm>
#include <span>
#include "walk.h"

namespace kitcambc {

struct Body {
    P pos{};
    float hp = 0;
    float dps = 0;    // damage a turn
    float burst = 0;  // damage in the next two turns
    int reach = 1;    // how far it hits
    float worth = 1;  // what a kill is worth
    int id = 0;
};

// Our strength over theirs around `at`. Above 1 we should win.
// Strength is health times damage. A body counts less the farther it is:
// ours 1 to 3 tiles, 0.7 to 5, 0.4 beyond; theirs 1 when nearly in reach.
inline double odds(P at, std::span<const Body> us, std::span<const Body> them, int ally_radius = 8, int sight = 9) {
    double our_hp = 0, our_dps = 0, their_hp = 0, their_dps = 0;
    for (const Body& u : us) {
        const int d = kit::cheb(u.pos, at);
        if (u.hp <= 0 || d > ally_radius) continue;
        const float w = d <= 3 ? 1.f : d <= 5 ? 0.7f : 0.4f;
        our_hp += w * u.hp, our_dps += w * u.dps;
    }
    for (const Body& e : them) {
        const int d = kit::cheb(e.pos, at);
        if (e.hp <= 0 || d > sight) continue;
        const float w = d <= e.reach + 1 ? 1.f : d <= 5 ? 0.7f : 0.4f;
        their_hp += w * e.hp, their_dps += w * e.dps;
    }
    return our_hp * our_dps / std::max(1.0, their_hp * their_dps);
}

// The damage we could take standing at `at`.
inline float exposure(P at, std::span<const Body> them) {
    float sum = 0;
    for (const Body& e : them)
        if (e.hp > 0 && kit::cheb(e.pos, at) <= e.reach) sum += e.burst;
    return sum;
}

// A kill's worth over the time it takes. Allies in reach count in full,
// allies two tiles short count less.
inline double kill_score(const Body& e, P from, int my_reach, std::span<const Body> us) {
    float burst = 0;
    for (const Body& u : us) {
        const int d = kit::cheb(u.pos, e.pos);
        if (u.hp > 0) burst += d <= u.reach ? u.burst : d <= u.reach + 2 ? 0.4727f * u.burst : 0.f;
    }
    const float turns = std::max(1.f, 2.f * e.hp / std::max(1.f, burst));
    double s = e.worth / turns;
    if (kit::cheb(e.pos, from) <= my_reach) s *= 1.382;
    return s;
}

// The enemy to focus: the best kill_score in sight. -1 when none.
inline int pick_target(P from, int my_reach, std::span<const Body> us, std::span<const Body> them, int sight = 9) {
    int best = -1;
    double best_s = -1e18;
    for (int i = 0; i < int(them.size()); ++i) {
        const Body& e = them[i];
        if (e.hp <= 0 || kit::cheb(e.pos, from) > sight) continue;
        const double s = kill_score(e, from, my_reach, us) - 1e-6 * e.id;
        if (s > best_s) best_s = s, best = i;
    }
    return best;
}

// After hitting: the tile beside us with less exposure. Prefers to keep the
// target within `keep`, then the tile nearer home. `pos` when none is better.
inline P kite(const Walker& w, P target, int keep, std::span<const Body> them, const Field& home) {
    const float here = exposure(w.pos, them);
    float best_x = here;
    int best_rank = 1 << 30;
    P best = w.pos;
    for (int k = 0; k < w.dirs; ++k) {
        const P t = w.pos + kDirs8[k];
        if (!w.free(t)) continue;
        const float x = exposure(t, them);
        const int rank = (kit::cheb(t, target) <= keep ? 0 : 1 << 20) + std::min(home[t], 1 << 19);
        if (x < best_x - 0.5f || (x <= best_x + 0.5f && rank < best_rank)) best_x = x, best_rank = rank, best = t;
    }
    return best_x < here ? best : w.pos;
}

// Running from a fight: least exposure, then farthest from the enemies, then nearest home.
inline P flee_step(const Walker& w, std::span<const Body> them, const Field& home) {
    struct Rank {
        float x;
        int gap, home;
        bool operator<(const Rank& o) const { return x != o.x ? x < o.x : gap != o.gap ? gap > o.gap : home < o.home; }
    };
    auto rank = [&](P p) {
        int d = 8;
        for (const Body& e : them)
            if (e.hp > 0) d = std::min(d, kit::cheb(p, e.pos));
        return Rank{exposure(p, them), d, home[p]};
    };
    Rank best = rank(w.pos);
    P to = w.pos;
    for (int k = 0; k < w.dirs; ++k) {
        const P n = w.pos + kDirs8[k];
        if (!w.free(n)) continue;
        const Rank r = rank(n);
        if (r < best) best = r, to = n;
    }
    return to;
}

}
