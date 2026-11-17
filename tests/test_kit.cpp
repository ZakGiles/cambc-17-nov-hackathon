// make test
#include <array>
#include <cstdint>
#include <cstdio>
#include <numeric>
#include <vector>
#include "kit/assign.h"
#include "kit/deadline.h"
#include "kit/field.h"
#include "kit/gamesearch.h"
#include "kit/grid.h"
#include "kit/rng.h"
#include "kit/svec.h"

using kit::P;

static int g_checks = 0, g_fails = 0;
#define CHECK(...)                                                             \
    do {                                                                       \
        ++g_checks;                                                            \
        if (!(__VA_ARGS__) && ++g_fails <= 20) std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #__VA_ARGS__); \
    } while (0)

// Relaxes until nothing changes: slow and obviously right.
template <class Cost>
static kit::Grid<int> slow_field(int w, int h, const std::vector<P>& sources, Cost&& cost, int dirs) {
    kit::Grid<int> d(w, h, kit::kFar);
    for (P s : sources) d[s] = 0;
    for (bool changed = true; changed;) {
        changed = false;
        for (int i = 0; i < w * h; ++i) {
            const P p = d.at(i);
            if (d[p] == kit::kFar) continue;
            for (int k = 0; k < dirs; ++k) {
                const P n = p + kit::kDirs8[k];
                if (!d.in(n)) continue;
                const int c = cost(n);
                if (c < 0 || d[p] + c >= d[n]) continue;
                d[n] = d[p] + c;
                changed = true;
            }
        }
    }
    return d;
}

static void test_grid() {
    CHECK(kit::manhattan({1, 2}, {4, -2}) == 7);
    CHECK(kit::cheb({1, 2}, {4, -2}) == 4);
    CHECK(kit::dist2({1, 2}, {4, -2}) == 25);
    kit::Grid<int> g(7, 5, 3);
    CHECK(g.size() == 35 && g[P{6, 4}] == 3);
    CHECK(g.in({0, 0}) && g.in({6, 4}) && !g.in({7, 4}) && !g.in({6, 5}) && !g.in({-1, 0}) && !g.in({0, -1}));
    for (int i = 0; i < g.size(); ++i) CHECK(g.idx(g.at(i)) == i);
    CHECK((P{1, 2} + P{3, 4}) == (P{4, 6}) && (P{1, 2} < P{2, 0}));
}

static void test_field() {
    kit::Rng rng(7);
    kit::Field f;
    for (int round = 0; round < 300; ++round) {
        const int w = rng.range(1, 12), h = rng.range(1, 12), dirs = rng.chance(0.5) ? 4 : 8;
        kit::Grid<unsigned char> wall(w, h, 0);
        kit::Grid<int> price(w, h, 1);
        for (int i = 0; i < w * h; ++i) {
            wall.a[i] = rng.chance(0.3);
            price.a[i] = rng.range(0, 9);
        }
        std::vector<P> sources;
        for (int k = rng.range(1, 3); k > 0; --k) sources.push_back({rng.below(w), rng.below(h)});
        auto open = [&](P p) { return !wall[p]; };

        const auto want = slow_field(w, h, sources, [&](P p) { return wall[p] ? -1 : 1; }, dirs);
        f.bfs(w, h, sources, open, dirs);
        CHECK(f.d.a == want.a);
        for (int i = 0; i < w * h; ++i) {
            const P p = want.at(i);
            const auto path = f.path(p, dirs);
            if (want[p] == kit::kFar) {
                CHECK(path.empty() && !f.reached(p));
                continue;
            }
            CHECK(int(path.size()) == want[p]);
            P at = p;
            for (P n : path) {
                CHECK((dirs == 4 ? kit::manhattan(at, n) : kit::cheb(at, n)) == 1);
                at = n;
            }
            CHECK(want[at] == 0);
        }

        const int cap = rng.range(0, 6);
        f.bfs(w, h, sources, open, dirs, cap);
        for (int i = 0; i < w * h; ++i) CHECK(f.d.a[i] == (want.a[i] <= cap ? want.a[i] : kit::kFar));

        auto cost = [&](P p) { return wall[p] ? -1 : price[p]; };
        f.dijkstra(w, h, sources, cost, dirs);
        CHECK(f.d.a == slow_field(w, h, sources, cost, dirs).a);

        kit::Grid<int> label;
        const int count = kit::regions(label, w, h, open, dirs);
        int seen = 0;
        for (int i = 0; i < w * h; ++i) {
            const P p = label.at(i);
            CHECK((label[p] == -1) == bool(wall[p]));
            if (wall[p]) continue;
            seen = std::max(seen, label[p] + 1);
            // Two open cells share a label exactly when one reaches the other.
            f.bfs(w, h, p, open, dirs);
            for (int j = 0; j < w * h; ++j)
                if (!wall.a[j]) CHECK((label.a[j] == label[p]) == (f.d.a[j] < kit::kFar));
        }
        CHECK(seen == count);
    }

    // A wall with one gap: the walk goes through the gap; a blocked target is still led up to.
    kit::Grid<unsigned char> wall(5, 5, 0);
    for (int y = 0; y < 4; ++y) wall[P{2, y}] = 1;
    f.bfs(5, 5, P{4, 0}, [&](P p) { return !wall[p]; }, 4);
    CHECK(f[P{0, 0}] == 12 && f[P{2, 4}] == 6 && f[P{9, 9}] == kit::kFar);
    CHECK(f.step(P{0, 4}, 4) == (P{1, 4}) && f.step(P{1, 4}, 4) == (P{2, 4}) && f.step(P{4, 0}, 4) == (P{4, 0}));
    // (1,3) and (0,4) are equally near from (0,3): close one and the walk takes the other.
    CHECK(f.step(P{0, 3}, [](P p) { return !(p == P{1, 3}); }, 4) == (P{0, 4}));
    CHECK(f.step(P{1, 4}, [](P p) { return !(p == P{2, 4}); }, 4) == (P{1, 4}));
    f.bfs(5, 5, P{2, 0}, [&](P p) { return !wall[p]; }, 4);
    CHECK(f[P{2, 0}] == 0 && f[P{1, 0}] == 1 && f[P{2, 1}] == kit::kFar);
}

static long long brute(int r, int rows, int cols, int left, unsigned used, const std::vector<std::vector<int>>& c) {
    if (left == 0) return 0;
    if (r == rows) return 1LL << 50;
    long long best = rows - r > left ? brute(r + 1, rows, cols, left, used, c) : 1LL << 50;
    for (int j = 0; j < cols; ++j)
        if (!(used >> j & 1)) best = std::min(best, c[r][j] + brute(r + 1, rows, cols, left - 1, used | 1u << j, c));
    return best;
}

static void test_assign() {
    kit::Rng rng(11);
    for (int round = 0; round < 2000; ++round) {
        const int rows = rng.range(0, 6), cols = rng.range(0, 6), want = std::min(rows, cols);
        std::vector<std::vector<int>> c(rows, std::vector<int>(cols));
        for (auto& row : c)
            for (int& x : row) x = rng.range(-20, 60);
        auto cost = [&](int r, int j) { return c[r][j]; };
        auto total = [&](const std::vector<int>& a, bool& valid) {
            long long sum = 0;
            unsigned used = 0;
            int n = 0;
            valid = int(a.size()) == rows;
            for (int r = 0; valid && r < rows; ++r) {
                if (a[r] == -1) continue;
                if (a[r] < 0 || a[r] >= cols || (used >> a[r] & 1)) valid = false;
                else used |= 1u << a[r], sum += c[r][a[r]], ++n;
            }
            valid = valid && n == want;
            return sum;
        };
        const long long best = brute(0, rows, cols, want, 0, c);
        bool valid = false;
        const long long exact = total(kit::assign_min(rows, cols, cost), valid);
        CHECK(valid && exact == best);
        const long long greedy = total(kit::assign_greedy(rows, cols, cost), valid);
        CHECK(valid && greedy >= best);
    }
}

static void test_rng_svec_deadline() {
    kit::Rng a(5), b(5), c(6);
    bool same = true, differs = false;
    for (int i = 0; i < 100; ++i) {
        const auto x = a.next();
        same = same && x == b.next();
        differs = differs || x != c.next();
    }
    CHECK(same && differs);
    std::array<int, 10> hits{};
    for (int i = 0; i < 100000; ++i) {
        const int v = a.below(10);
        CHECK(v >= 0 && v < 10);
        if (v >= 0 && v < 10) ++hits[v];
        const int r = a.range(-3, 3);
        CHECK(r >= -3 && r <= 3);
        const double u = a.unit();
        CHECK(u >= 0.0 && u < 1.0);
    }
    for (int n : hits) CHECK(n > 9500 && n < 10500);
    std::vector<int> v(50);
    std::iota(v.begin(), v.end(), 0);
    a.shuffle(v);
    bool moved = false;
    long long sum = 0;
    for (int i = 0; i < 50; ++i) moved = moved || v[i] != i, sum += v[i];
    CHECK(moved && sum == 49 * 50 / 2);

    kit::SVec<int, 3> s;
    CHECK(s.empty() && s.push(1) && s.push(2) && s.push(3) && !s.push(4) && s.full() && s.size() == 3);
    s.drop(0);
    CHECK(s.size() == 2 && s[0] == 3 && s.back() == 2);
    int total = 0;
    for (int x : s) total += x;
    CHECK(total == 5);

    CHECK(kit::Deadline(0).passed());
    CHECK(!kit::Deadline(60000).passed() && kit::Deadline(60000).left_ms() > 50000);
}

struct Ttt {
    using Move = int;
    std::array<std::int8_t, 9> b{};
    int turn = 0;

    int player() const { return turn; }
    int winner() const {
        static constexpr int lines[8][3] = {{0, 1, 2}, {3, 4, 5}, {6, 7, 8}, {0, 3, 6}, {1, 4, 7}, {2, 5, 8}, {0, 4, 8}, {2, 4, 6}};
        for (const auto& l : lines)
            if (b[l[0]] && b[l[0]] == b[l[1]] && b[l[1]] == b[l[2]]) return b[l[0]];
        return 0;
    }
    bool over() const {
        if (winner()) return true;
        for (int x : b)
            if (!x) return false;
        return true;
    }
    void moves(std::vector<int>& out) const {
        if (winner()) return;
        for (int i = 0; i < 9; ++i)
            if (!b[i]) out.push_back(i);
    }
    void play(int m) { b[m] = std::int8_t(turn + 1), turn ^= 1; }
    double result(int p) const {
        const int w = winner();
        return w == 0 ? 0.5 : w == p + 1 ? 1.0 : 0.0;
    }
    int eval() const {
        const int w = winner();
        return w == 0 ? 0 : w == turn + 1 ? kit::kWin : -kit::kWin;
    }
};

// Games lost by the searcher against random play, half of them moving second.
template <class Choose>
static int losses_to_random(int games, Choose&& choose) {
    kit::Rng rng(3);
    int lost = 0;
    std::vector<int> legal;
    for (int game = 0; game < games; ++game) {
        Ttt g;
        const int me = game & 1;
        while (!g.over()) {
            if (g.player() == me) {
                g.play(choose(g));
            } else {
                legal.clear();
                g.moves(legal);
                g.play(rng.pick(legal));
            }
        }
        lost += g.result(me) == 0.0;
    }
    return lost;
}

static void test_gamesearch() {
    kit::AlphaBeta<Ttt> ab;
    kit::Mcts<Ttt> mcts;

    Ttt empty;
    ab.best(empty, 60000);
    CHECK(ab.score == 0 && ab.depth == 9);

    // X . .      X to move wins at 2; if it were O's move, O must block there.
    // X O O      (X also wins at 6, which is the move alpha-beta and MCTS must find.)
    // . . .
    Ttt win;
    win.b = {1, 0, 0, 1, 2, 2, 0, 0, 0};
    CHECK(ab.best(win, 60000) == 6 && ab.score >= kit::kWin - 9);
    CHECK(mcts.best(win, 60000, 3000) == 6 && mcts.value > 0.9);

    // O to move must block at 6 or lose.
    Ttt block;
    block.b = {1, 0, 0, 1, 2, 0, 0, 0, 0};
    block.turn = 1;
    CHECK(ab.best(block, 60000) == 6);
    CHECK(mcts.best(block, 60000, 3000) == 6);

    CHECK(losses_to_random(200, [&](const Ttt& g) { return ab.best(g, 60000); }) == 0);
    CHECK(losses_to_random(100, [&](const Ttt& g) { return mcts.best(g, 60000, 2000); }) == 0);

    // Out of time at once: still a legal move.
    const int quick_ab = ab.best(empty, 0), quick_mcts = mcts.best(empty, 0);
    CHECK(quick_ab >= 0 && quick_ab < 9 && quick_mcts >= 0 && quick_mcts < 9 && mcts.iterations == 1);
}

int main() {
    test_grid();
    test_field();
    test_assign();
    test_rng_svec_deadline();
    test_gamesearch();
    std::printf("%d checks, %d failed\n", g_checks, g_fails);
    return g_fails ? 1 : 0;
}
