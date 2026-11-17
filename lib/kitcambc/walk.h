#pragma once
// Moving one unit past others. From cambc's navigator.
//
// Each pick returns the tile to move to, or `pos` when there is none.
// The game makes the move. Then call moved(p), or refused(p) and pick again.
// walk() does that loop.
#include <algorithm>
#include <vector>
#include "../kit/field.h"

namespace kitcambc {

using kit::Field;
using kit::Grid;
using kit::kDirs8;
using kit::kFar;
using kit::P;

// How well step d points along `to`. Higher is better.
inline float heading(P d, P to) {
    const float dot = float(d.x * to.x + d.y * to.y);
    return d.x != 0 && d.y != 0 ? dot * 0.70710678f : dot;
}

struct Walker {
    const Grid<unsigned char>* wall = nullptr;  // 1 = cannot enter
    Grid<unsigned char> occ;                    // 1 = a unit stands here
    Grid<unsigned char> danger;                 // 1 = do not walk in from a safe tile
    P pos{}, last{-1, -1};                      // `last` stops stepping back and forth
    int dirs = 8;

    // Call at the start of each turn. Then block() every other unit.
    void start(const Grid<unsigned char>& walls, P at) {
        wall = &walls;
        occ.reset(walls.w, walls.h, 0);
        danger.reset(walls.w, walls.h, 0);
        if (!(at == pos)) last = pos, pos = at;
    }
    void block(P p) {
        if (occ.in(p)) occ[p] = 1;
    }
    void mark_danger(P centre, int radius) {
        for (int y = centre.y - radius; y <= centre.y + radius; ++y)
            for (int x = centre.x - radius; x <= centre.x + radius; ++x)
                if (danger.in({x, y})) danger[P{x, y}] = 1;
    }

    bool free(P p) const { return wall->in(p) && !(*wall)[p] && !occ[p]; }
    bool risky(P p) const { return danger.in(p) && danger[p]; }
    int gap(P a, P b) const { return dirs == 4 ? kit::manhattan(a, b) : kit::cheb(a, b); }

    void moved(P p) { last = pos, pos = p; }
    void refused(P p) { block(p); }

    // One step down the field. Never uphill. A level step only with `sideways`,
    // and never back to `last`. Order: safe tiles, lower value, not `last`,
    // then the heading to `aim`. Stays put rather than walk into danger.
    P step(const Field& f, bool sideways, P aim = {-1, -1}) const {
        const int here = f[pos];
        int best_v = kFar, best_rank = 1 << 30;
        float best_h = -1e9f;
        P best = pos;
        for (int k = 0; k < dirs; ++k) {
            const P n = pos + kDirs8[k];
            if (!free(n)) continue;
            const int v = f[n];
            if (v >= kFar || v > here || (v == here && (!sideways || n == last))) continue;
            const int rank = (risky(n) ? 2 : 0) + (n == last ? 1 : 0);
            const float h = aim.x >= 0 ? heading(kDirs8[k], aim - pos) : 0.f;
            const bool safer = rank / 2 < best_rank / 2;
            const bool same = rank / 2 == best_rank / 2;
            if (safer || (same && (v < best_v || (v == best_v && (rank < best_rank || (rank == best_rank && h > best_h)))))) {
                best_v = v, best_rank = rank, best_h = h, best = n;
            }
        }
        if (!(best == pos) && best_rank >= 2 && !risky(pos)) return pos;
        return best;
    }

    // Downhill if possible, else sideways around the blocker.
    P travel(const Field& f, P aim = {-1, -1}) const {
        const P n = step(f, false, aim);
        return n == pos ? step(f, true, aim) : n;
    }

    // Straight at a goal, with no field. Cheap, but walls stop it.
    P toward(P goal) const {
        const int here = gap(pos, goal);
        const bool safe_here = !risky(pos);
        int best_d = here, best_e = 1 << 30;
        P best = pos;
        for (int k = 0; k < dirs; ++k) {
            const P n = pos + kDirs8[k];
            if (!free(n) || n == last || (safe_here && risky(n))) continue;
            const int d = gap(n, goal), e = kit::dist2(n, goal);
            if (d < best_d || (d == best_d && d < here && e < best_e)) best_d = d, best_e = e, best = n;
        }
        return best;
    }

    // A short search that treats units as walls. Use it when travel() is stuck.
    P go_to(const std::vector<P>& goals, int depth = 24) {
        open_.clear();
        for (P g : goals) {
            if (g == pos) return pos;
            if (free(g)) open_.push_back(g);
        }
        if (open_.empty()) return pos;
        local_.bfs(wall->w, wall->h, open_, [&](P p) { return free(p) || p == pos; }, dirs, depth);
        return travel(local_);
    }

    // travel(), with a detour around units when stuck. The detour is kept for
    // `depth` turns, so the unit does not step back into the jam. Pass the
    // field's sources as `goals` and a turn counter.
    P route(const Field& f, const std::vector<P>& goals, int turn, P aim = {-1, -1}, int depth = 24) {
        if (turn <= detour_until_) {
            const int left = local_[pos];
            const P n = left > 0 && left < kFar ? travel(local_) : pos;
            if (!(n == pos)) return n;
            detour_until_ = -1;
        }
        P n = step(f, false, aim);
        if (!(n == pos)) return sideways_ = 0, n;
        if (sideways_ < 2 && !((n = step(f, true, aim)) == pos)) return ++sideways_, n;
        n = go_to(goals, depth);
        if (!(n == pos)) return detour_until_ = turn + depth, sideways_ = 0, n;
        return step(f, true, aim);
    }

    // Away from the threats. `home` is a field to safety.
    // homeward: go home if that does not bring a threat nearer. Else gain distance.
    P flee(const std::vector<P>& threats, const Field& home, bool homeward = false) const {
        auto away = [&](P p) {
            int d = 1 << 20;
            for (P t : threats) d = std::min(d, gap(p, t));
            return d;
        };
        const int here = away(pos), here_h = home[pos];
        if (homeward) {
            int best_h = here_h, best_d = -1;
            P best = pos;
            for (int k = 0; k < dirs; ++k) {
                const P n = pos + kDirs8[k];
                if (!free(n)) continue;
                const int d = away(n), h = home[n];
                if (d < here || h >= here_h || h > best_h || (h == best_h && d <= best_d)) continue;
                best_h = h, best_d = d, best = n;
            }
            if (!(best == pos)) return best;
        }
        int best_d = here, best_h = here_h;
        P best = pos;
        for (int k = 0; k < dirs; ++k) {
            const P n = pos + kDirs8[k];
            if (!free(n)) continue;
            const int d = away(n), h = home[n];
            if (d > best_d || (d == best_d && h < best_h)) best_d = d, best_h = h, best = n;
        }
        return best;
    }

  private:
    Field local_;
    std::vector<P> open_;
    int detour_until_ = -1, sideways_ = 0;
};

// pick() -> P chooses a tile. move(P) -> bool asks the game. A refused tile is
// marked as taken and the pick runs again. True when the unit moved.
template <class Pick, class Move>
bool walk(Walker& w, Pick&& pick, Move&& move) {
    for (int tries = 0; tries < 8; ++tries) {
        const P n = pick();
        if (n == w.pos) return false;
        if (move(n)) {
            w.moved(n);
            return true;
        }
        w.refused(n);
    }
    return false;
}

}
