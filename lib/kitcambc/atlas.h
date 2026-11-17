#pragma once
// Walking distances between the map's points of interest, measured once.
// From cambc's atlas. Plans then cost a table read, not a search.
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>
#include "../kit/field.h"

namespace kitcambc {

struct Atlas {
    static constexpr int kFar = 0xFFFF;

    // One search from each point. pass(P) says whether a tile can be entered.
    // Spread the calls over turns if the compute limit is tight: measure_one().
    template <class Pass>
    void measure(int w, int h, std::span<const kit::P> points, Pass&& pass, int dirs = 8) {
        reset(int(points.size()));
        for (int g = 0; g < n_; ++g) measure_one(w, h, points, g, pass, dirs);
    }

    void reset(int n) {
        n_ = n, done_ = 0;
        tri_.assign(std::size_t(n) * (n > 0 ? n - 1 : 0) / 2, std::uint16_t(kFar));
    }
    // Fills point g's row. Call for g = 0, 1, 2, ... in order.
    template <class Pass>
    void measure_one(int w, int h, std::span<const kit::P> points, int g, Pass&& pass, int dirs = 8) {
        field_.bfs(w, h, points[g], pass, dirs);
        for (int o = 0; o < g; ++o) tri_[at(g, o)] = std::uint16_t(std::min(field_[points[o]], int(kFar)));
        done_ = g + 1;
    }

    int size() const { return n_; }
    bool has(int g) const { return g >= 0 && g < done_; }
    // Steps from point a to point b. kFar when unmeasured or unreachable.
    int between(int a, int b) const {
        if (a == b) return 0;
        if (!has(a) || !has(b)) return kFar;
        return a > b ? tri_[at(a, b)] : tri_[at(b, a)];
    }

  private:
    static std::size_t at(int g, int o) { return std::size_t(g) * (g - 1) / 2 + o; }
    int n_ = 0, done_ = 0;
    std::vector<std::uint16_t> tri_;  // the lower triangle: row g holds points 0..g-1
    kit::Field field_;
};

}
