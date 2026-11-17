// s3: team main v4 (build-14) with taxi size 4 and frontier scan 8-16. Beats build-14 on frontier and lair.
// Drags each cow one step up a precomputed slope. Walks to the nearest cow worth dragging.
//
// Fuel: a read costs 133, a move 410, a loop step about 8. The turn has 20,000.
// A turn that runs out resumes in the next one with stale positions, and its
// moves then do harm. So the bot counts what it spends (`spent`), decides
// first, moves only if the turn has not changed, and re-counts cells with
// what is left.
#include <cstring>
#include "maps.h"
#include "phais.hpp"

namespace {

using phais::Direction;

constexpr int N = 14, CELLS = N * N, BOTS = 8, STAY = 4;
constexpr int kFarCap = 24;
// Fuel estimates, on the high side.
constexpr int kRead = 140, kMove = 420, kNear = 350, kNearScan = 1300, kFar = 250 + kFarCap * 50, kBotlet = 200;
constexpr int kBudget = 19000;                       // of 20,000; cut-offs start near 22,000 by this count
constexpr int kMovesCost = kRead + BOTS * kMove;     // the turn check and eight moves
constexpr int kScanCost = kRead + 50;
// Cells re-counted a turn, least and most, by map. Fewer is better on checkerboard: tested 0 to 32.
constexpr int kScanLo[kMaps] = {4,8,16}, kScanHi[kMaps] = {8,16,32};
int kScanMin = 16, kScanMax = 32;
int kKeep = kMovesCost + 16 * kScanCost;             // fuel a search must leave
constexpr int kRest = 2;                             // turns a botlet waits after finding nothing
constexpr int kTaxiFarM[3] = {6,6,6}; int kTaxiFar = 6;                          // a target this far is worth a ride
constexpr int kTaxiSizeM[3] = {4,4,4}; int kTaxiSize = 3;                         // botlets that make a ride
// Rides pay where work is far apart. On checkerboard cows are everywhere and gathering wastes turns.
constexpr bool kTaxiOn[kMaps] = {false, true, true};
constexpr int kRallyWaitM[3] = {4,4,4}; constexpr int kRallyReach = 3; int kRallyWait = 4;       // steps to walk to a ride; turns to wait for the others
// Lock (from the teammate's s2): pull against the drag an enemy botlet on our cows will make.
constexpr bool kLockOn[kMaps] = {true, false, true};
constexpr int kLockCost = 2 * BOTS * kRead + 1200;

const signed char* PV = nullptr;     // productivity
const unsigned char* WAY = nullptr;  // the way to drag a cow from a cell; STAY = leave it
const signed char* GAIN = nullptr;   // what that step is worth
unsigned char C[CELLS];              // cows we believe are on a cell
unsigned char L[CELLS], WH[CELLS];   // the cells with cows, and each cell's place in L
int LN = 0;
unsigned char claim[CELLS];          // botlets walking to a cell
const unsigned char (*NEAR)[kNearN] = nullptr;  // per cell, the nearest cells worth a look; null on an unseen map
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
int spent = 0;                // fuel used this turn, estimated
int mv_way[BOTS], mv_drag[BOTS];  // this turn's moves; STAY = none
int rest[BOTS];               // turns before this botlet searches again
int ride[BOTS];               // the cell this botlet rides a cow to, -1 none
int ride_at[BOTS];            // where it was when it last pulled: a ride that does not move is over
bool done[BOTS];              // this turn's move is set
int uc[BOTS + 1], un[BOTS + 1], nu = 0;  // cells dragged from this turn, and cows taken from each
int rally = -1, rally_since = 0;         // the cell far walkers gather on, and since which turn

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
        PV = kP[m], WAY = kWay[m], GAIN = kGain[m], HOT = kHot[m], HOTN = kHotN[m], NEAR = kNearCells[m];
        kTaxiFar = kTaxiFarM[m], kTaxiSize = kTaxiSizeM[m], kRallyWait = kRallyWaitM[m], kScanMin = kScanLo[m], kScanMax = kScanHi[m], kKeep = kMovesCost + kScanMin * kScanCost;
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
        PV = p_seen, WAY = way_seen, GAIN = gain_seen, HOT = hot_seen, HOTN = CELLS;
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

// The nearest cell to fetch a cow from, from the precomputed list. -1 when none.
inline int find_listed(int me) {
    for (int k = 0; k < kNearN; ++k) {
        const int c = NEAR[me][k];
        if (c == 255) break;
        if (C[c] > claim[c]) return c;
    }
    return -1;
}

// The same by looking at every cell within two steps: for a map with no list.
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

inline int dist(int a, int b) {
    const int dx = a % N - b % N, dy = a / N - b / N;
    return (dx < 0 ? -dx : dx) + (dy < 0 ? -dy : dy);
}

int taken(int c) {
    for (int k = 0; k < nu; ++k)
        if (uc[k] == c) return un[k];
    return 0;
}

// The index of the next cow on c that no botlet of ours drags this turn.
int take(int c) {
    for (int k = 0; k < nu; ++k)
        if (uc[k] == c) return un[k]++;
    uc[nu] = c, un[nu++] = 1;
    return 0;
}

inline int toward(int from, int to) {
    const int dx = to % N - from % N, dy = to / N - from / N;
    const int adx = dx < 0 ? -dx : dx, ady = dy < 0 ? -dy : dy;
    return adx >= ady ? (dx > 0 ? 1 : 3) : (dy > 0 ? 0 : 2);
}

// A ride stops this many steps from its goal: one from an enemy cell, so the
// cow we rode is not left where it scores for them.
inline int short_of(int to) { return PV[to] < 0 ? 1 : 0; }

// A ride: the k botlets in who[], all on c, pull one cow toward `to`. Pulls add
// up, so the cow and every botlet that pulled move up to k cells this turn.
// Botlets not needed pull against each other in pairs, to stay with the cow.
void pull(const int* who, int k, int c, int to) {
    const int idx = take(c);
    const int x = c % N, y = c / N;
    const int dx = to % N - x, dy = to / N - y;
    const int ax = dx < 0 ? -dx : dx, ay = dy < 0 ? -dy : dy;
    const int need = ax + ay - short_of(to);
    const int a = k < need ? k : need;
    int px = ax + ay > 0 ? (a * ax + (ax + ay) / 2) / (ax + ay) : 0;
    if (px > ax) px = ax;
    int py = a - px;
    if (py > ay) py = ay, px = a - py;
    const int wx = dx > 0 ? 1 : 3, wy = dy > 0 ? 0 : 2;
    // A pull off the board does not count, so pair up along an axis with room both ways.
    const int pa = x > 0 && x < N - 1 ? 1 : y > 0 && y < N - 1 ? 0 : -1;
    int n = 0;
    for (; n < k; ++n) {
        const int i = who[n];
        done[i] = true;
        ride_at[i] = c;
        mv_drag[i] = idx;
        if (n < px) mv_way[i] = wx;
        else if (n < px + py) mv_way[i] = wy;
        else if (pa >= 0 && ((n - px - py) & 1 || n + 1 < k)) mv_way[i] = (n - px - py) & 1 ? pa + 2 : pa;
        else mv_way[i] = toward(c, to), mv_drag[i] = -1, ride[i] = -1;  // the odd one out walks
    }
    drag_from[who[0]] = c;
}

void play() {
    for (int i = 0; i < BOTS; ++i) {
        const int x = phais::get_bot_x(0, i), y = phais::get_bot_y(0, i);
        const int c = x < 0 || y < 0 || x >= N || y >= N ? -1 : y * N + x;
        const int from = drag_from[i];
        if (from >= 0 && from != c && C[from] > 0) set_cows(from, C[from] - 1);
        drag_from[i] = -1;
        at[i] = c;
        done[i] = c < 0;
        mv_way[i] = STAY;
    }
    for (int i = 0; i < BOTS; ++i) {
        bool seen = at[i] < 0;
        for (int j = 0; j < i && !seen; ++j) seen = at[j] == at[i];
        if (!seen) set_cows(at[i], phais::get_cows(at[i] % N, at[i] / N));
    }
    spent += 2 * BOTS * kRead + BOTS * (kRead + 60) + 900;  // 900: the ride loop below
    nu = 0;

    // Lock: an enemy botlet stands with one of ours on a cell with cows. It will
    // drag one. Guess the drag from our own table turned half round, and pull
    // the same cow the other way: the pulls cancel and the cow stays.
    if (map_id >= 0 && kLockOn[map_id] && spent + kLockCost + kKeep < kBudget) {
        spent += kLockCost;
        int ec[BOTS];
        for (int j = 0; j < BOTS; ++j) {
            const int ex = phais::get_bot_x(1, j), ey = phais::get_bot_y(1, j);
            ec[j] = ex < 0 || ey < 0 || ex >= N || ey >= N ? -1 : ey * N + ex;
        }
        for (int i = 0; i < BOTS; ++i) {
            const int c = at[i];
            if (done[i] || ride[i] >= 0 || C[c] == 0) continue;
            int ne = 0;
            for (int j = 0; j < BOTS; ++j) ne += ec[j] == c;
            if (ne > C[c]) ne = C[c];
            if (taken(c) >= ne) continue;
            const int w = kWay[map_id][CELLS - 1 - c];
            if (w == STAY) continue;
            const int e = (w + 2) & 3;  // their pull, in our frame
            const int nx = c % N + (e == 1) - (e == 3), ny = c / N + (e == 0) - (e == 2);
            if (nx < 0 || ny < 0 || nx >= N || ny >= N) continue;
            if (PV[ny * N + nx] >= PV[c]) continue;  // their pull does not hurt us
            drop_target(i);
            mv_way[i] = (e + 2) & 3, mv_drag[i] = take(c);
            drag_from[i] = c;
            done[i] = true;
        }
    }

    // Rides under way.
    for (int i = 0; i < BOTS; ++i) {
        if (done[i] || ride[i] < 0) continue;
        const int c = at[i];
        int who[BOTS], k = 0;
        for (int j = i; j < BOTS; ++j)
            if (!done[j] && ride[j] == ride[i] && at[j] == c) who[k++] = j;
        const int to = ride[i];
        if (k < 2 || dist(c, to) <= short_of(to) || C[c] == 0 || ride_at[i] == c) {
            for (int n = 0; n < k; ++n) ride[who[n]] = -1;
            continue;
        }
        pull(who, k, c, to);
    }

    // Each other botlet: drag the cow here up the slope, or walk to one worth dragging.
    // The first in line changes each turn, so no botlet is always last to the fuel.
    for (int q = 0; q < BOTS; ++q) {
        const int i = (q + turn_no) & 7;
        if (done[i]) continue;
        const int c = at[i];
        spent += kBotlet;
        if (WAY[c] != STAY && taken(c) < C[c]) {
            drop_target(i);
            mv_way[i] = WAY[c], mv_drag[i] = take(c);
            drag_from[i] = c;
            done[i] = true;
            continue;
        }
        if (tgt[i] >= 0 && (tgt[i] == c || C[tgt[i]] == 0)) drop_target(i);
        int t = tgt[i];
        const int x = c % N, y = c / N;
        if (t < 0 && rest[i] > 0) --rest[i];
        else if (t < 0) {
            const bool few = LN <= kFarCap + 8;  // few cells with cows: look at them all
            bool looked = false;
            if (!few && NEAR) spent += kNear, t = find_listed(c);
            else if (!few && spent + kNearScan + kKeep < kBudget) spent += kNearScan, t = find_near(x, y, c);
            if (t < 0 && spent + kFar + kKeep < kBudget) spent += kFar, t = find_far(x, y, c), looked = true;
            if (t >= 0) tgt[i] = t, ++claim[t];
            else if (looked) rest[i] = kRest;
        }
        if (t >= 0) mv_way[i] = toward(c, t), mv_drag[i] = -1;
    }

    // Far to go: gather on a cell with a spare cow and ride it.
    int far[BOTS], nf = 0, lead = -1;
    for (int i = 0; i < BOTS; ++i) {
        if (done[i]) continue;
        const bool is_far = tgt[i] >= 0 && dist(at[i], tgt[i]) >= kTaxiFar;
        if (is_far && lead < 0) lead = i;
        if (is_far || tgt[i] < 0) far[nf++] = i;  // idle botlets come along
    }
    const int rally_cost = 600 + (rally < 0 ? nf * nf * 40 : nf * 60);
    if (lead >= 0 && nf >= kTaxiSize && map_id >= 0 && kTaxiOn[map_id] && spent + rally_cost + kMovesCost < kBudget) {
        spent += rally_cost;
        // The gathering cell: where the most of them already stand on a spare cow.
        if (rally >= 0 && (C[rally] <= taken(rally) || turn_no - rally_since > 3 * kRallyWait)) rally = -1;
        if (rally < 0) {
            int best_n = 0;
            for (int a = 0; a < nf; ++a) {
                const int c = at[far[a]];
                if (C[c] <= taken(c)) continue;
                int n = 0;
                for (int b = 0; b < nf; ++b) n += dist(at[far[b]], c) <= kRallyReach;
                if (n > best_n) best_n = n, rally = c;
            }
            rally_since = turn_no;
            if (best_n < kTaxiSize) rally = -1;
        }
        if (rally >= 0) {
            int who[BOTS], k = 0, coming = 0;
            for (int a = 0; a < nf; ++a) {
                const int i = far[a], d = dist(at[i], rally);
                if (d == 0) who[k++] = i;
                else if (d <= kRallyReach) ++coming, mv_way[i] = toward(at[i], rally), mv_drag[i] = -1;
            }
            const bool go = k >= kTaxiSize && (coming == 0 || turn_no - rally_since >= kRallyWait);
            if (go) {
                const int to = tgt[lead] >= 0 && dist(rally, tgt[lead]) >= kTaxiFar ? tgt[lead] : -1;
                if (to >= 0) {
                    for (int n = 0; n < k; ++n) drop_target(who[n]), ride[who[n]] = to, rest[who[n]] = 0;
                    pull(who, k, rally, to);
                }
                rally = -1;
            } else {
                for (int n = 0; n < k; ++n) mv_way[who[n]] = STAY;  // wait for the others
            }
        }
    } else {
        rally = -1;
    }

    // Late: the turn ran out while we thought. These moves are stale. Give the turn up.
    if (phais::get_turn() != turn_no) {
        for (int i = 0; i < BOTS; ++i) drag_from[i] = -1;
        return;
    }
    for (int i = 0; i < BOTS; ++i)
        if (mv_way[i] != STAY) phais::submit_move(i, static_cast<Direction>(mv_way[i]), mv_drag[i]);
    spent += kMovesCost;

    // The enemy moves cows too: re-count some cells with the fuel left.
    int n = (kBudget - spent) / kScanCost;
    if (n > kScanMax) n = kScanMax;
    if (n > HOTN) n = HOTN;
    for (; n > 0; --n) {
        if (hot_at >= HOTN) hot_at = 0;
        const int c = HOT[hot_at++];
        set_cows(c, phais::get_cows(c % N, c / N));
    }
}

}

int main() {
    for (int i = 0; i < BOTS; ++i) tgt[i] = drag_from[i] = ride[i] = ride_at[i] = -1;
    for (;;) {
        turn_no = phais::get_turn();
        spent = kRead;
        if (map_id == -2) identify(), spent += 6000;  // the table copies are dear
        if (map_id >= 0 || !learn()) play();
        phais::end_turn();
    }
}
