#pragma once
// Points, directions, distances and a W*H array.
#include <algorithm>
#include <array>
#include <compare>
#include <cstddef>
#include <vector>

namespace kit {

struct P {
    int x = 0, y = 0;
    constexpr P operator+(P o) const { return {x + o.x, y + o.y}; }
    constexpr P operator-(P o) const { return {x - o.x, y - o.y}; }
    constexpr P operator*(int k) const { return {x * k, y * k}; }
    constexpr P& operator+=(P o) { return x += o.x, y += o.y, *this; }
    constexpr auto operator<=>(const P&) const = default;
};

// The first four of kDirs8 are the orthogonal steps, so `dirs` = 4 or 8 picks the neighbourhood.
inline constexpr std::array<P, 8> kDirs8{P{0, -1}, P{1, 0}, P{0, 1}, P{-1, 0},
                                         P{1, -1}, P{1, 1}, P{-1, 1}, P{-1, -1}};

constexpr int iabs(int v) { return v < 0 ? -v : v; }
constexpr int manhattan(P a, P b) { return iabs(a.x - b.x) + iabs(a.y - b.y); }
constexpr int cheb(P a, P b) { return std::max(iabs(a.x - b.x), iabs(a.y - b.y)); }
constexpr int dist2(P a, P b) { return (a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y); }

// Use Grid<unsigned char> for masks: std::vector<bool> has no real references.
template <class T>
struct Grid {
    int w = 0, h = 0;
    std::vector<T> a;

    Grid() = default;
    Grid(int w_, int h_, T fill_ = T{}) : w(w_), h(h_), a(std::size_t(w_) * h_, fill_) {}

    void reset(int w_, int h_, T fill_ = T{}) {
        w = w_, h = h_;
        a.assign(std::size_t(w) * h, fill_);
    }
    void fill(T v) { std::fill(a.begin(), a.end(), v); }

    bool in(P p) const { return unsigned(p.x) < unsigned(w) && unsigned(p.y) < unsigned(h); }
    int idx(P p) const { return p.y * w + p.x; }
    P at(int i) const { return {i % w, i / w}; }
    int size() const { return w * h; }

    T& operator[](P p) { return a[idx(p)]; }
    const T& operator[](P p) const { return a[idx(p)]; }
};

}
