#pragma once
// Time-bounded search for two-player games: UCT Monte Carlo tree search and
// iterative-deepening alpha-beta. A game G provides:
//
//   using Move = ...;                          // cheap to copy, default-constructible
//   int  player() const;                       // 0 or 1: who moves next
//   void moves(std::vector<Move>& out) const;  // appends the legal moves; none once over
//   void play(Move m);
//   bool over() const;
//   double result(int p) const;  // Mcts only: at the end, 1 win / 0.5 draw / 0 loss for p
//   int  eval() const;           // AlphaBeta only: score for the player to move,
//                                // +kWin / -kWin when the game is decided, and
//                                // well inside that range otherwise
//
// G is copied once per simulated move, so keep it small. A player may move
// twice in a row. Use Mcts when no good eval exists, AlphaBeta when one does.
#include <algorithm>
#include <climits>
#include <cmath>
#include <cstddef>
#include <utility>
#include <vector>
#include "deadline.h"
#include "rng.h"

namespace kit {

inline constexpr int kWin = 1'000'000;

template <class G>
struct Mcts {
    using Move = typename G::Move;
    struct Node {
        Move move{};
        int parent = -1;
        int mover = -1;    // who played `move`
        int first = -1;    // first child; -1 until expanded
        int count = 0;     // children
        int visits = 0;
        double score = 0;  // sum of results for `mover`
    };

    Rng rng;
    double explore = 1.4;
    std::size_t max_nodes = 4'000'000;

    // Of the last call:
    int iterations = 0;
    double value = 0;  // mean result of the move returned, for the player who moves at the root

    std::vector<Node> nodes;

    // Always runs one iteration, so the move returned is legal if any is.
    Move best(const G& root, double ms, int max_iterations = INT_MAX) {
        const Deadline deadline(ms);
        nodes.clear();
        nodes.push_back(Node{});
        for (iterations = 0; iterations < max_iterations; ++iterations) {
            if (iterations > 0 && deadline.passed()) break;
            G g = root;
            int v = 0;
            while (nodes[v].count > 0) {
                v = pick(v);
                g.play(nodes[v].move);
            }
            if (nodes[v].first < 0 && nodes.size() < max_nodes && !g.over()) {
                buf_.clear();
                g.moves(buf_);
                const int first = int(nodes.size()), mover = g.player();
                nodes[v].first = first;
                nodes[v].count = int(buf_.size());
                for (const Move& m : buf_) nodes.push_back(Node{m, v, mover});
                if (!buf_.empty()) {
                    v = first + rng.below(int(buf_.size()));
                    g.play(nodes[v].move);
                }
            }
            while (!g.over()) {
                buf_.clear();
                g.moves(buf_);
                if (buf_.empty()) break;
                g.play(buf_[rng.below(int(buf_.size()))]);
            }
            for (int u = v; u > 0; u = nodes[u].parent) {
                ++nodes[u].visits;
                nodes[u].score += g.result(nodes[u].mover);
            }
            ++nodes[0].visits;
        }
        int top = -1;
        for (int c = nodes[0].first, e = c + nodes[0].count; c < e; ++c)
            if (top < 0 || nodes[c].visits > nodes[top].visits) top = c;
        if (top < 0) return Move{};
        value = nodes[top].visits ? nodes[top].score / nodes[top].visits : 0.5;
        return nodes[top].move;
    }

  private:
    std::vector<Move> buf_;

    int pick(int v) {
        const double log_n = std::log(double(nodes[v].visits) + 1.0);
        int top = -1;
        double top_value = -1.0;
        for (int c = nodes[v].first, e = c + nodes[v].count; c < e; ++c) {
            if (nodes[c].visits == 0) return c;
            const double x = nodes[c].score / nodes[c].visits + explore * std::sqrt(log_n / nodes[c].visits);
            if (x > top_value) top_value = x, top = c;
        }
        return top;
    }
};

template <class G>
struct AlphaBeta {
    using Move = typename G::Move;

    // Of the last call:
    int depth = 0;       // deepest search that finished
    int score = 0;       // of the move returned, for the player who moves at the root
    long long visited = 0;

    // Returns the best move of the deepest search that finished within ms.
    Move best(const G& root, double ms, int max_depth = 64) {
        deadline_ = Deadline(ms);
        out_ = false;
        depth = 0, score = 0, visited = 0;
        bufs_.resize(std::size_t(max_depth) + 2);

        std::vector<Move> legal;
        root.moves(legal);
        if (legal.empty()) return Move{};
        std::vector<std::pair<int, Move>> order;
        for (const Move& m : legal) order.emplace_back(0, m);
        Move chosen = legal[0];

        for (int d = 1; d <= max_depth && !out_; ++d) {
            int alpha = -kInf, done = 0;
            Move now = order[0].second;
            cut_ = false;
            for (auto& [s, m] : order) {
                const int v = child(root, m, d, alpha, kInf, 0);
                if (out_) break;
                s = v;
                ++done;
                if (v > alpha) alpha = v, now = m;
            }
            // The last best move is searched first, so a cut-short pass that
            // finished it can only have found the same move or a better one.
            if (done == 0) break;
            chosen = now, score = alpha;
            if (out_) break;
            depth = d;
            std::stable_sort(order.begin(), order.end(), [](const auto& a, const auto& b) { return a.first > b.first; });
            // Nothing deeper to see: every line ran to the end of the game, or one side has a forced win.
            if (!cut_ || alpha >= kWin - max_depth || alpha <= -kWin + max_depth) break;
        }
        return chosen;
    }

  private:
    static constexpr int kInf = INT_MAX / 2;
    Deadline deadline_{0};
    bool out_ = false;
    bool cut_ = false;  // this pass stopped a line at the depth limit
    std::vector<std::vector<Move>> bufs_;

    int child(const G& g, const Move& m, int d, int alpha, int beta, int ply) {
        G c = g;
        c.play(m);
        return c.player() == g.player() ? search(c, d - 1, alpha, beta, ply + 1)
                                        : -search(c, d - 1, -beta, -alpha, ply + 1);
    }

    int search(const G& g, int d, int alpha, int beta, int ply) {
        if ((++visited & 255) == 0 && deadline_.passed()) out_ = true;
        if (out_) return 0;
        if (g.over()) return leaf(g, ply);
        if (d == 0) {
            cut_ = true;
            return leaf(g, ply);
        }
        std::vector<Move>& legal = bufs_[ply];
        legal.clear();
        g.moves(legal);
        if (legal.empty()) return leaf(g, ply);
        int top = -kInf;
        for (const Move& m : legal) {
            const int v = child(g, m, d, alpha, beta, ply);
            if (out_) return 0;
            top = std::max(top, v);
            alpha = std::max(alpha, v);
            if (alpha >= beta) break;
        }
        return top;
    }

    // A win sooner beats a win later; a loss later beats a loss sooner.
    static int leaf(const G& g, int ply) {
        const int e = g.eval();
        return e >= kWin ? e - ply : e <= -kWin ? e + ply : e;
    }
};

}
