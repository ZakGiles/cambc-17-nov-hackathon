#pragma once
// Who goes where: match rows (units) to columns (targets), each column used once.
#include <algorithm>
#include <tuple>
#include <vector>

namespace kit {

// The matching with the lowest total cost (Hungarian, O(n^2 m)). cost(row, col)
// returns an int; give pairs that must not happen a large cost such as 1e7 and
// test the result. Returns each row's column, -1 for rows left over when
// rows > cols.
template <class Cost>
std::vector<int> assign_min(int rows, int cols, Cost&& cost) {
    std::vector<int> out(rows, -1);
    if (rows == 0 || cols == 0) return out;
    const bool flip = rows > cols;
    const int n = flip ? cols : rows, m = flip ? rows : cols;
    auto at = [&](int i, int j) -> long long { return flip ? cost(j, i) : cost(i, j); };

    const long long inf = 1LL << 60;
    std::vector<long long> u(n + 1, 0), v(m + 1, 0), minv(m + 1);
    std::vector<int> p(m + 1, 0), way(m + 1, 0);
    std::vector<char> used(m + 1);
    for (int i = 1; i <= n; ++i) {
        p[0] = i;
        int j0 = 0;
        std::fill(minv.begin(), minv.end(), inf);
        std::fill(used.begin(), used.end(), 0);
        do {
            used[j0] = 1;
            const int i0 = p[j0];
            long long delta = inf;
            int j1 = 0;
            for (int j = 1; j <= m; ++j) {
                if (used[j]) continue;
                const long long cur = at(i0 - 1, j - 1) - u[i0] - v[j];
                if (cur < minv[j]) minv[j] = cur, way[j] = j0;
                if (minv[j] < delta) delta = minv[j], j1 = j;
            }
            for (int j = 0; j <= m; ++j) {
                if (used[j]) u[p[j]] += delta, v[j] -= delta;
                else minv[j] -= delta;
            }
            j0 = j1;
        } while (p[j0] != 0);
        do {
            const int j1 = way[j0];
            p[j0] = p[j1];
            j0 = j1;
        } while (j0 != 0);
    }
    for (int j = 1; j <= m; ++j) {
        if (p[j] == 0) continue;
        if (flip) out[j - 1] = p[j] - 1;
        else out[p[j] - 1] = j - 1;
    }
    return out;
}

// Cheapest pair first, O(rc log rc). Not optimal, but close when targets are spread out.
template <class Cost>
std::vector<int> assign_greedy(int rows, int cols, Cost&& cost) {
    std::vector<std::tuple<long long, int, int>> pairs;
    pairs.reserve(std::size_t(rows) * cols);
    for (int r = 0; r < rows; ++r)
        for (int c = 0; c < cols; ++c) pairs.emplace_back(cost(r, c), r, c);
    std::sort(pairs.begin(), pairs.end());
    std::vector<int> out(rows, -1);
    std::vector<char> taken(cols, 0);
    for (const auto& [value, r, c] : pairs) {
        if (out[r] != -1 || taken[c]) continue;
        out[r] = c;
        taken[c] = 1;
    }
    return out;
}

}
