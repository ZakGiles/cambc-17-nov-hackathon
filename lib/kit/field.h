#pragma once
// Distance fields on a grid: BFS, Dijkstra, walking down a field, regions.
//
// To move units to a target, compute one field FROM the target and let every
// unit step to its lowest neighbour: one search serves all units.
#include <functional>
#include <queue>
#include <span>
#include <utility>
#include <vector>
#include "grid.h"

namespace kit {

inline constexpr int kFar = 1 << 29;

struct Field {
    Grid<int> d;  // kFar where unreached
    std::vector<int> queue;

    // Steps to the nearest source. pass(P) says whether a cell can be entered.
    // A source counts as reached even when pass refuses it, so a field can lead
    // up to a blocked target. dirs is 4 or 8; cells past max_dist stay kFar.
    template <class Pass>
    void bfs(int w, int h, std::span<const P> sources, Pass&& pass, int dirs = 8, int max_dist = kFar) {
        clear(w, h);
        queue.clear();
        for (P s : sources)
            if (d.in(s) && d[s] != 0) {
                d[s] = 0;
                queue.push_back(d.idx(s));
            }
        for (std::size_t head = 0; head < queue.size(); ++head) {
            const P p = d.at(queue[head]);
            const int next = d[p] + 1;
            if (next > max_dist) break;
            for (int k = 0; k < dirs; ++k) {
                const P n = p + kDirs8[k];
                if (!d.in(n) || d[n] != kFar || !pass(n)) continue;
                d[n] = next;
                queue.push_back(d.idx(n));
            }
        }
    }
    template <class Pass>
    void bfs(int w, int h, P source, Pass&& pass, int dirs = 8, int max_dist = kFar) {
        bfs(w, h, std::span<const P>(&source, 1), pass, dirs, max_dist);
    }

    // Cheapest total to the nearest source. cost(P) is the price of entering a
    // cell, negative for blocked.
    template <class Cost>
    void dijkstra(int w, int h, std::span<const P> sources, Cost&& cost, int dirs = 8) {
        clear(w, h);
        using Item = std::pair<int, int>;
        std::priority_queue<Item, std::vector<Item>, std::greater<Item>> heap;
        for (P s : sources)
            if (d.in(s) && d[s] != 0) {
                d[s] = 0;
                heap.push({0, d.idx(s)});
            }
        while (!heap.empty()) {
            const auto [dist, i] = heap.top();
            heap.pop();
            if (dist != d.a[i]) continue;
            const P p = d.at(i);
            for (int k = 0; k < dirs; ++k) {
                const P n = p + kDirs8[k];
                if (!d.in(n)) continue;
                const int c = cost(n);
                if (c < 0 || dist + c >= d[n]) continue;
                d[n] = dist + c;
                heap.push({dist + c, d.idx(n)});
            }
        }
    }
    template <class Cost>
    void dijkstra(int w, int h, P source, Cost&& cost, int dirs = 8) {
        dijkstra(w, h, std::span<const P>(&source, 1), cost, dirs);
    }

    int operator[](P p) const { return d.in(p) ? d[p] : kFar; }
    bool reached(P p) const { return (*this)[p] < kFar; }

    // The neighbour of `from` with the lowest value among those ok(P) allows
    // (pass a test for cells other units stand on); `from` when none is lower.
    template <class Ok>
    P step(P from, Ok&& ok, int dirs = 8) const {
        P best = from;
        int low = (*this)[from];
        for (int k = 0; k < dirs; ++k) {
            const P n = from + kDirs8[k];
            const int v = (*this)[n];
            if (v < low && ok(n)) low = v, best = n;
        }
        return best;
    }
    P step(P from, int dirs = 8) const {
        return step(from, [](P) { return true; }, dirs);
    }

    // The cells after `from` down to a source. Empty when unreached or already there.
    std::vector<P> path(P from, int dirs = 8) const {
        std::vector<P> out;
        if (!reached(from)) return out;
        for (P p = from; (*this)[p] > 0;) {
            const P n = step(p, dirs);
            if (n == p) break;
            out.push_back(p = n);
        }
        return out;
    }

  private:
    void clear(int w, int h) {
        if (d.w != w || d.h != h) d.reset(w, h, kFar);
        else d.fill(kFar);
    }
};

// Numbers each connected region of passable cells from 0; -1 elsewhere. Returns the count.
template <class Pass>
int regions(Grid<int>& label, int w, int h, Pass&& pass, int dirs = 8) {
    label.reset(w, h, -1);
    std::vector<int> stack;
    int count = 0;
    for (int i = 0; i < w * h; ++i) {
        if (label.a[i] != -1 || !pass(label.at(i))) continue;
        label.a[i] = count;
        stack.push_back(i);
        while (!stack.empty()) {
            const P p = label.at(stack.back());
            stack.pop_back();
            for (int k = 0; k < dirs; ++k) {
                const P n = p + kDirs8[k];
                if (!label.in(n) || label[n] != -1 || !pass(n)) continue;
                label[n] = count;
                stack.push_back(label.idx(n));
            }
        }
        ++count;
    }
    return count;
}

}
