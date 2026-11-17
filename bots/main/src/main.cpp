// Drags each cow one step up a precomputed slope. Walks to the nearest cow worth dragging.
// Fuel: a read costs 133, a move 410, a loop step about 8. The turn has 20,000.
#include <cstring>
#include "maps.h"
#include "phais.hpp"

namespace {

using phais::Direction;

constexpr int N = 14, CELLS = N * N, BOTS = 8, STAY = 4;
constexpr int kNearSearches = 5, kFarSearches = 2, kFarCap = 24;
constexpr int kScan = 24;  // cells re-counted a turn: the enemy moves cows too

const unsigned char* WAY = nullptr;  // the way to drag a cow from a cell; STAY = leave it
const signed char* GAIN = nullptr;   // what that step is worth
unsigned char C[CELLS];              // cows we believe are on a cell
unsigned char L[CELLS], WH[CELLS];   // the cells with cows, and each cell's place in L
int LN = 0;
unsigned char claim[CELLS];          // botlets walking to a cell
const unsigned char* HOT = nullptr;  // cells worth re-counting: a cow there can be dragged
int HOTN = 0, hot_at = 0;

// For a map we have not seen: tables filled in the first turns.
signed char p_seen[CELLS], gain_seen[CELLS];
unsigned char way_seen[CELLS], hot_seen[CELLS];

int map_id = -2;  // -2 not looked at yet, -1 unknown
int turn_no = 0;
int tgt[BOTS];        // the cell a botlet walks to, -1 none
int drag_from[BOTS];  // the cell it dragged from last turn, -1 none
int at[BOTS];         // its cell this turn, -1 off the board
int scan_at = 0;

void set_cows(int c, int n) {
    const int old = C[c];
    if (old == n) return;
    if (old == 0) {
        WH[c] = static_cast<unsigned char>(LN);
        L[LN++] = static_cast<unsigned char>(c);
    } else if (n == 0) {
        const int k = WH[c], last = L[--LN];
        L[k] = static_cast<unsigned char>(last);
        WH[last] = static_cast<unsigned char>(k);
    }
    C[c] = static_cast<unsigned char>(n);
}

void identify() {
    static constexpr int probe[4] = {1, 4 * N + 4, 13 * N, 7 * N + 6};
    int seen[4];
    for (int k = 0; k < 4; ++k) seen[k] = phais::get_productivity(probe[k] % N, probe[k] / N);
    map_id = -1;
    for (int m = 0; m < kMaps; ++m) {
        bool same = true;
        for (int k = 0; k < 4; ++k) same = same && kP[m][probe[k]] == seen[k];
        if (!same) continue;
        map_id = m;
        WAY = kWay[m], GAIN = kGain[m], HOT = kHot[m], HOTN = kHotN[m];
        std::memcpy(C, kC[m], CELLS);
        std::memcpy(L, kList[m], CELLS);
        std::memcpy(WH, kWhere[m], CELLS);
        LN = kListN[m];
        return;
    }
}

// An unseen map: read it over five turns. True while still reading.
bool learn() {
    if (turn_no < 2) {
        for (int c = turn_no * 98, e = c + 98; c < e; ++c) p_seen[c] = static_cast<signed char>(phais::get_productivity(c % N, c / N));
        return true;
    }
    if (turn_no == 2) {
        static constexpr int step[4] = {N, 1, -N, -1};
        for (int c = 0; c < CELLS; ++c) {
            const int x = c % N, y = c / N;
            const bool ok[4] = {y < N - 1, x < N - 1, y > 0, x > 0};
            int way = STAY, gain = 0;
            for (int d = 0; d < 4; ++d)
                if (ok[d] && p_seen[c + step[d]] - p_seen[c] > gain) gain = p_seen[c + step[d]] - p_seen[c], way = d;
            way_seen[c] = static_cast<unsigned char>(way);
            gain_seen[c] = static_cast<signed char>(gain);
            hot_seen[c] = static_cast<unsigned char>(c);
        }
        WAY = way_seen, GAIN = gain_seen, HOT = hot_seen, HOTN = CELLS;
        return true;
    }
    if (turn_no < 5) {
        for (int c = (turn_no - 3) * 98, e = c + 98; c < e; ++c) set_cows(c, phais::get_cows(c % N, c / N));
        return true;
    }
    return false;
}

inline int score(int c, int x, int y) {
    const int cx = c % N, cy = c / N;
    const int d = (cx > x ? cx - x : x - cx) + (cy > y ? cy - y : y - cy);
    return GAIN[c] * 64 / (d + 1);
}

// The best cell within two steps to fetch a cow from. -1 when none.
int find_near(int x, int y, int me) {
    int best = -1, best_s = 0;
    const int x0 = x > 2 ? x - 2 : 0, x1 = x < N - 3 ? x + 2 : N - 1;
    const int y0 = y > 2 ? y - 2 : 0, y1 = y < N - 3 ? y + 2 : N - 1;
    for (int yy = y0; yy <= y1; ++yy)
        for (int c = yy * N + x0, e = yy * N + x1; c <= e; ++c) {
            if (GAIN[c] <= 0 || C[c] <= claim[c] || c == me) continue;
            const int s = score(c, x, y);
            if (s > best_s) best_s = s, best = c;
        }
    return best;
}

// The same over the cells with cows. Looks at kFarCap of them a call, in turn.
int find_far(int x, int y, int me) {
    int best = -1, best_s = 0, k = scan_at;
    for (int i = LN < kFarCap ? LN : kFarCap; i > 0; --i) {
        if (k >= LN) k = 0;
        const int c = L[k++];
        if (GAIN[c] <= 0 || C[c] <= claim[c] || c == me) continue;
        const int s = score(c, x, y);
        if (s > best_s) best_s = s, best = c;
    }
    scan_at = k;
    return best;
}

void drop_target(int i) {
    if (tgt[i] < 0) return;
    --claim[tgt[i]];
    tgt[i] = -1;
}

// Lair: all eight ride one cow to the enemy's corner in three turns.
bool lair_train() {
    if (map_id != 2 || turn_no > 2) return false;
    const int want = turn_no * (4 * N + 4);
    for (int i = 0; i < BOTS; ++i)
        if (at[i] != want) return false;
    if (C[want] == 0) return false;
    static constexpr Direction last[BOTS] = {Direction::E, Direction::E, Direction::N, Direction::N,
                                             Direction::E, Direction::W, Direction::N, Direction::S};
    for (int i = 0; i < BOTS; ++i)
        phais::submit_move(i, turn_no == 2 ? last[i] : i < 4 ? Direction::E : Direction::N, 0);
    drag_from[0] = want;
    return true;
}

void play() {
    for (int i = 0; i < BOTS; ++i) {
        const int x = phais::get_bot_x(0, i), y = phais::get_bot_y(0, i);
        const int c = x < 0 || y < 0 || x >= N || y >= N ? -1 : y * N + x;
        const int from = drag_from[i];
        if (from >= 0 && from != c && C[from] > 0) set_cows(from, C[from] - 1);
        drag_from[i] = -1;
        at[i] = c;
    }
    for (int i = 0; i < BOTS; ++i) {
        bool seen = at[i] < 0;
        for (int j = 0; j < i && !seen; ++j) seen = at[j] == at[i];
        if (!seen) set_cows(at[i], phais::get_cows(at[i] % N, at[i] / N));
    }
    if (lair_train()) return;
    for (int k = HOTN < kScan ? HOTN : kScan; k > 0; --k) {
        if (hot_at >= HOTN) hot_at = 0;
        const int c = HOT[hot_at++];
        set_cows(c, phais::get_cows(c % N, c / N));
    }

    int near = 0, far = 0;
    for (int i = 0; i < BOTS; ++i) {
        const int c = at[i];
        if (c < 0) continue;
        const int here = C[c];
        if (here > 0 && WAY[c] != STAY) {
            int used = 0;
            for (int j = 0; j < i; ++j) used += drag_from[j] == c;
            if (used < here) {
                drop_target(i);
                phais::submit_move(i, static_cast<Direction>(WAY[c]), used);
                drag_from[i] = c;
                continue;
            }
        }
        if (tgt[i] >= 0 && (tgt[i] == c || C[tgt[i]] == 0)) drop_target(i);
        int t = tgt[i];
        const int x = c % N, y = c / N;
        if (t < 0 && near < kNearSearches) {
            ++near;
            t = find_near(x, y, c);
            if (t < 0 && far < kFarSearches) ++far, t = find_far(x, y, c);
            if (t >= 0) tgt[i] = t, ++claim[t];
        }
        if (t < 0) continue;
        const int dx = t % N - x, dy = t / N - y;
        const int adx = dx < 0 ? -dx : dx, ady = dy < 0 ? -dy : dy;
        phais::submit_move(i, adx >= ady ? (dx > 0 ? Direction::E : Direction::W) : (dy > 0 ? Direction::N : Direction::S));
    }
}

}

int main() {
    for (int i = 0; i < BOTS; ++i) tgt[i] = drag_from[i] = -1;
    for (;;) {
        turn_no = phais::get_turn();
        if (map_id == -2) identify();
        if (map_id >= 0 || !learn()) play();
        phais::end_turn();
    }
}
