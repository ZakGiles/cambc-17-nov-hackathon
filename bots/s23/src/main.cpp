// s23: s22 with Swedish fish #124 openings on all three maps.
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
constexpr int kScanLo[kMaps] = {2,8,16}, kScanHi[kMaps] = {16,16,32};
int kScanMin = 16, kScanMax = 32;
int kKeep = kMovesCost + 16 * kScanCost;             // fuel a search must leave
constexpr int kRestM[3] = {1,2,2}; int kRest = 2;                             // turns a botlet waits after finding nothing
constexpr int kTaxiFarM[3] = {6,9,6}; int kTaxiFar = 6;                          // a target this far is worth a ride
// Botlets that make a ride, by map. On frontier 3 scores 30k more than 4 over the pool.
constexpr int kTaxiSizes[kMaps] = {4, 4, 3};
int kTaxiSize = 3;
// Rides pay where work is far apart. On checkerboard cows are everywhere and gathering wastes turns.
constexpr bool kTaxiOn[kMaps] = {false,true,true};
constexpr int kRallyWaitM[3] = {4,4,4}; constexpr int kRallyReach = 3; int kRallyWait = 4;       // steps to walk to a ride; turns to wait for the others
// Lock (from the teammate's s2): pull against the drag an enemy botlet on our cows will make.
// On where it can pay. In a game it turns itself off once it has failed more than it has worked:
// against a bot that dodges it only wastes turns, against one that does not it is worth 60k to 400k.
constexpr bool kLockOn[kMaps] = {true, false, true};
constexpr bool kLockAdapt[kMaps] = {true, false, false};  // on lair it is better left on: -44k when it could turn off
int kLockGrace = 2;
constexpr int kLockCost = 2 * BOTS * kRead + 1200;
bool kChain = true;                       // checkerboard: drag toward the next cow. Tested: worse over the pool
constexpr int kShyM[3] = {30,0,10}; int kShyTurns = 10;                        // turns we dodge on a cell where a drag was cancelled

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
int uc[BOTS + 1], un[BOTS + 1], ur[BOTS + 1], nu = 0;  // cells dragged from this turn; cows taken from the front and the back
bool plain[BOTS];             // last turn's move was an ordinary drag
int lock_cell[BOTS], lock_cows[BOTS];  // where this botlet locked last turn, and the cows there then
bool lock_live = true, lock_adapt = true, cb_taxi = false;  // switched by the opening read
int lock_ok = 0, lock_bad = 0;         // locks that held, and locks that did not
// Lair games freeze: our locks cancel their pulls and they keep trying. The score then
// runs on at a steady rate. So after a long freeze work out the final score: the lead
// now, plus the lead gained a turn times the turns left. If that loses, stop locking
// for a while to shake the position.
constexpr int kFrozenLocks = 7, kFrozenCheck = 30, kLooseTurns = 25;  // seven of eight locking for 60 turns is a freeze
int frozen_run = 0, frozen_us = 0, frozen_them = 0, loose_until = -1;
int max_turns = 500;
int shy_cell[BOTS], shy_until[BOTS];  // a cell where our drag was cancelled: someone pulls against us there
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
    // A new map can match four cells by chance, and the wrong tables score nothing. Check twelve more.
    static constexpr int check[12] = {0, 13, 182, 195, 45, 150, 143, 52, 97, 98, 35, 160};
    int seen[4];
    for (int k = 0; k < 4; ++k) seen[k] = phais::get_productivity(probe[k] % N, probe[k] / N);
    map_id = -1;
    for (int m = 0; m < kMaps; ++m) {
        bool same = true;
        for (int k = 0; k < 4; ++k) same = same && kP[m][probe[k]] == seen[k];
        for (int k = 0; k < 12 && same; ++k) {
            const int c = check[k];
            same = kP[m][c] == phais::get_productivity(c % N, c / N);
        }
        if (!same) continue;
        map_id = m;
        PV = kP[m], WAY = kWay[m], GAIN = kGain[m], HOT = kHot[m], HOTN = kHotN[m], NEAR = kNearCells[m];
        kScanMin = kScanLo[m], kScanMax = kScanHi[m], kKeep = kMovesCost + kScanMin * kScanCost;
        kTaxiSize = kTaxiSizes[m];
        kShyTurns = kShyM[m]; kRest = kRestM[m]; kTaxiFar = kTaxiFarM[m]; kRallyWait = kRallyWaitM[m];
        std::memcpy(C, kC[m], CELLS);
        std::memcpy(L, kList[m], CELLS);
        std::memcpy(WH, kWhere[m], CELLS);
        LN = kListN[m];
        return;
    }
}

// An unseen map: read it over six turns, half the board a turn. True while still reading.
// Steps are counted, not taken from the turn number, so a late turn skips nothing.
int learn_step = 0;
bool learn() {
    if (learn_step >= 6) return false;
    const int half = learn_step & 1, c0 = half * 98, c1 = c0 + 98;
    if (learn_step < 2) {
        for (int c = c0; c < c1; ++c) p_seen[c] = static_cast<signed char>(phais::get_productivity(c % N, c / N));
    } else if (learn_step < 4) {
        static constexpr int step[4] = {N, 1, -N, -1};
        for (int c = c0; c < c1; ++c) {
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
    } else {
        for (int c = c0; c < c1; ++c) set_cows(c, phais::get_cows(c % N, c / N));
    }
    ++learn_step;
    return true;
}

inline int score(int c, int x, int y) {
    const int cx = c % N, cy = c / N;
    const int d = (cx > x ? cx - x : x - cx) + (cy > y ? cy - y : y - cy);
    return GAIN[c] * 64 / (d + 1);
}

// The nearest cell to fetch a cow from, from the precomputed list. -1 when none.
inline int find_listed(int me) {
    // Nearest first. Among cells as near, the one with the most free cows: a botlet
    // can step in and out of a stack with no walking between drags.
    const int mx = me % N, my = me / N;
    int best = -1, best_d = 99, best_n = 0;
    for (int k = 0; k < kNearN; ++k) {
        const int c = NEAR[me][k];
        if (c == 255) break;
        const int n = C[c] - claim[c];
        if (n <= 0) continue;
        const int cx = c % N, cy = c / N;
        const int d = (cx > mx ? cx - mx : mx - cx) + (cy > my ? cy - my : my - cy);
        if (d > best_d) break;  // the list is sorted by distance
        if (d < best_d || (n > best_n && map_id == 0)) best = c, best_d = d, best_n = n;  // stacks: checkerboard only
    }
    return best;
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
        if (uc[k] == c) return un[k] + ur[k];
    return 0;
}

// The index of the next cow on c that no botlet of ours drags this turn.
int take(int c) {
    for (int k = 0; k < nu; ++k)
        if (uc[k] == c) return un[k]++;
    uc[nu] = c, ur[nu] = 0, un[nu++] = 1;
    return 0;
}

// The same from the back: a locker guesses we take the first cow.
int take_last(int c) {
    for (int k = 0; k < nu; ++k)
        if (uc[k] == c) return C[c] - 1 - ur[k]++;
    uc[nu] = c, un[nu] = 0, ur[nu++] = 1;
    return C[c] - 1;
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

// Checkerboard: a cow on c can go to more than one of our cells. Take the one
// with another enemy-side cow beside it, so the next drag needs no walk.
int chain_way(int c) {
    static constexpr int step[4] = {N, 1, -N, -1};
    const int x = c % N, y = c / N;
    const bool ok[4] = {y < N - 1, x < N - 1, y > 0, x > 0};
    const bool more_here = C[c] - taken(c) > 1;  // a cow stays behind: step back for it
    int best = WAY[c], best_s = -1;
    for (int d = 0; d < 4; ++d) {
        if (!ok[d]) continue;
        const int q = c + step[d];
        if (PV[q] <= PV[c]) continue;
        int sc = d == WAY[c] ? 1 : 0;
        if (more_here) sc += 4;
        const int qx = q % N, qy = q / N;
        const bool okq[4] = {qy < N - 1, qx < N - 1, qy > 0, qx > 0};
        for (int e = 0; e < 4; ++e) {
            if (!okq[e]) continue;
            const int m = q + step[e];
            if (m != c && PV[m] < 0 && C[m] > claim[m]) sc += 2;
        }
        if (sc > best_s) best_s = sc, best = d;
    }
    return best;
}

// A second way to drag a cow from c to a better cell, STAY when there is none.
int other_way(int c) {
    static constexpr int step[4] = {N, 1, -N, -1};
    const int x = c % N, y = c / N;
    const bool ok[4] = {y < N - 1, x < N - 1, y > 0, x > 0};
    for (int d = 0; d < 4; ++d)
        if (d != WAY[c] && ok[d] && PV[c + step[d]] > PV[c]) return d;
    return STAY;
}

// Frontier: some bots beat our rides there (all eight still home at turn 6, or two pairs on the
// edges at (7,13) and (13,7)). Against those, drop rides and scan a fixed 12 cells: that wins the map.
bool taxi_off = false;
bool astra_fr = false;  // frontier: Astra's opening seen at turn 6
bool lair_one = false;  // lair: one enemy cell, (11,11), at turn 4
void read_opponent() {
    int cells[BOTS], n = 0;
    for (int j = 0; j < BOTS; ++j) {
        const int ex = phais::get_bot_x(1, j), ey = phais::get_bot_y(1, j);
        const int c = ex < 0 || ey < 0 || ex >= N || ey >= N ? -1 : ey * N + ex;
        bool seen = false;
        for (int k = 0; k < n; ++k) seen |= cells[k] == c;
        if (!seen) cells[n++] = c;
    }
    bool home = n == 1 && cells[0] == 13 * N + 13;
    bool edges = n == 2 && ((cells[0] == 13 * N + 7 && cells[1] == 7 * N + 13) || (cells[1] == 13 * N + 7 && cells[0] == 7 * N + 13));
    const bool astra = n == 2 && ((cells[0] == 11 * N + 9 && cells[1] == 10 * N + 10) || (cells[1] == 11 * N + 9 && cells[0] == 10 * N + 10));
    if (astra) {  // two pairs at (9,11) and (10,10): small rides from far, a wide scan
        astra_fr = true;
        kTaxiSize = 4, kTaxiFar = 10, kShyTurns = 0, kScanMin = 8, kScanMax = 32, kRest = 1, kRallyWait = 8; taxi_off = false;
        kKeep = kMovesCost + kScanMin * kScanCost;
    }
    if (home || edges) {
        taxi_off = true;
        kRallyWait = 6;
        kScanMin = kScanMax = 12;
        kKeep = kMovesCost + kScanMin * kScanCost;
    }
    // Known frontier openings at turn 6 and what beats each bot there.
    struct Fw { int n; int cells[8]; int ts, far, shy, lo, hi, rest, rally; bool off; };
    static const Fw kFw[] = {
        {4, {10 * N + 10, 11 * N + 10, 9 * N + 11, 10 * N + 11}, 3, 8, 10, 2, 32, 1, 4, false},  // Swedish fish #117
        {3, {13 * N + 7, 13 * N + 8, 13 * N + 9}, 2, 9, 10, 8, 16, 2, 2, false},  // NEETCoders #118
        {1, {10 * N + 10}, 4, 10, 0, 12, 16, 2, 8, false},  // Swedish fish #124
        {2, {13 * N + 7, 7 * N + 13}, 3, 10, 0, 8, 8, 1, 4, true},  // Mite
        {2, {13 * N + 8, 8 * N + 13}, 4, 12, 10, 8, 8, 4, 4, false},  // bentheboss
        {1, {13 * N + 13}, 4, 4, 5, 8, 32, 3, 4, true},  // Swedish fish: all home
    };
    for (const Fw& s : kFw) {
        if (s.n != n) continue;
        bool all = true;
        for (int k = 0; k < n && all; ++k) {
            bool hit = false;
            for (int q = 0; q < n; ++q) hit |= cells[q] == s.cells[k];
            all = hit;
        }
        if (!all) continue;
        kTaxiSize = s.ts, kTaxiFar = s.far, kShyTurns = s.shy, kScanMin = s.lo, kScanMax = s.hi, kRest = s.rest, kRallyWait = s.rally;
        taxi_off = s.off;
        kKeep = kMovesCost + kScanMin * kScanCost;
        break;
    }
}

void play() {
    if (map_id == 1 && turn_no == 6) read_opponent();
    if (map_id == 1 && turn_no == 14 && astra_fr) {
        // Astra's newer bots (#98 on) stand on exactly (4,10), (6,8) and (7,7) here; the older ones elsewhere.
        static const int kNew[3] = {10 * N + 4, 8 * N + 6, 7 * N + 7};
        bool all = true, seen[3] = {false, false, false};
        for (int j = 0; j < BOTS; ++j) {
            const int c = phais::get_bot_y(1, j) * N + phais::get_bot_x(1, j);
            bool in = false;
            for (int k = 0; k < 3; ++k) if (kNew[k] == c) in = seen[k] = true;
            all &= in;
        }
        if (all && seen[0] && seen[1] && seen[2]) {
            kTaxiSize = 2, kTaxiFar = 9, kShyTurns = 0, kScanMin = 2, kScanMax = 24, kRest = 2, kRallyWait = 8; taxi_off = false;
            kKeep = kMovesCost + kScanMin * kScanCost;
        }
    }
    if (map_id == 2 && turn_no == 8 && lair_one) {
        // Swedish fish #124 stands all on (9,9) at turn 8; the bots sharing its turn-6 cell stand on two cells.
        bool all = true;
        for (int j = 0; j < BOTS; ++j) all &= phais::get_bot_y(1, j) * N + phais::get_bot_x(1, j) == 9 * N + 9;
        if (all) {
            kTaxiSize = 8, kTaxiFar = 9, kShyTurns = 10, kScanMin = 2, kScanMax = 16, kRest = 4, kRallyWait = 6; taxi_off = false, lock_live = false;
            kKeep = kMovesCost + kScanMin * kScanCost;
        }
    }
    if (map_id == 2 && turn_no == 6 && lair_one) {
        // Swedish fish #117 and Battlecode bots #57/#82 all stand on (11,11) at turn 4; at turn 6 #117 is on three cells.
        static const int kFish[3] = {10 * N + 10, 11 * N + 10, 10 * N + 11};
        bool all = true, seen[3] = {false, false, false};
        for (int j = 0; j < BOTS; ++j) {
            const int c = phais::get_bot_y(1, j) * N + phais::get_bot_x(1, j);
            bool in = false;
            for (int k = 0; k < 3; ++k) if (kFish[k] == c) in = seen[k] = true;
            all &= in;
        }
        if (all && seen[0] && seen[1] && seen[2]) {
            kTaxiSize = 2, kTaxiFar = 10, kShyTurns = 10, kScanMin = 6, kScanMax = 16, kRest = 1, kRallyWait = 6; taxi_off = false, lock_live = false;
            kKeep = kMovesCost + kScanMin * kScanCost;
        }
    }
    if (map_id == 2 && turn_no == 4) {
        // Lair: known openings at turn 4 and what beats each bot there.
        struct Lw { int n; int cells[8]; int ts, far, shy, lo, hi, rest, rally; bool off, live; };
        static const Lw kLw[] = {
            {2, {3 * N + 2, 2 * N + 3}, 5, 10, 30, 4, 24, 3, 4, false, true},
            {3, {11 * N + 11, 12 * N + 11, 11 * N + 12}, 5, 8, 10, 4, 32, 4, 2, false, false},
        };
        int cells[BOTS], n = 0;
        for (int j = 0; j < BOTS; ++j) {
            const int c = phais::get_bot_y(1, j) * N + phais::get_bot_x(1, j);
            bool seen = false;
            for (int k = 0; k < n; ++k) seen |= cells[k] == c;
            if (!seen) cells[n++] = c;
        }
        lair_one = n == 1 && cells[0] == 11 * N + 11;
        for (const Lw& s : kLw) {
            if (s.n != n) continue;
            bool all = true;
            for (int k = 0; k < n && all; ++k) {
                bool hit = false;
                for (int q = 0; q < n; ++q) hit |= cells[q] == s.cells[k];
                all = hit;
            }
            if (!all) continue;
            kTaxiSize = s.ts, kTaxiFar = s.far, kShyTurns = s.shy, kScanMin = s.lo, kScanMax = s.hi, kRest = s.rest, kRallyWait = s.rally;
            taxi_off = s.off, lock_live = s.live;
            kKeep = kMovesCost + kScanMin * kScanCost;
            break;
        }
    }
    if (map_id == 0 && turn_no == 4) {
        // Checkerboard: a bot that keeps all eight home this long out-drags our usual settings.
        int ecell[BOTS];
        bool home = true;
        for (int j = 0; j < BOTS; ++j) {
            const int ex = phais::get_bot_x(1, j), ey = phais::get_bot_y(1, j);
            ecell[j] = ey * N + ex;
            home &= ex == 13 && ey == 13;
        }

        if (home) {
            kScanMin = 4, kScanMax = 16, kRest = 3, kLockGrace = 6, kShyTurns = 60, kChain = false;
            kKeep = kMovesCost + kScanMin * kScanCost;
        }
        // Known openings at turn 4, in our frame: cells the enemy stands on, and what beats that bot.
        struct Sw { int n; int cells[8]; int lo, hi, rest, grace, shy; bool chain, live, adapt, taxi; int ts, far; };
        static const Sw kSw[] = {
            {7, {13 * N + 9, 12 * N + 10, 11 * N + 11, 13 * N + 11, 10 * N + 12, 9 * N + 13, 11 * N + 13}, 6, 32, 4, 0, 10, true, true, true, true, 3, 6},  // Astra #119
            {6, {13 * N + 9, 12 * N + 10, 11 * N + 11, 10 * N + 12, 12 * N + 12, 9 * N + 13}, 8, 16, 2, 8, 60, true, true, true, true, 4, 6},  // Swedish fish #124
            {4, {13 * N + 10, 12 * N + 11, 11 * N + 12, 12 * N + 13}, 6, 24, 1, 20, 120, true, true, false, true, 3, 9},  // bentheboss #79
            {1, {13 * N + 13}, 2, 24, 4, 8, 5, false, false, true, true, 4, 4},  // Swedish fish #52, #66: all home
            {6, {12 * N + 10, 11 * N + 11, 10 * N + 12, 11 * N + 12, 9 * N + 13, 13 * N + 13}, 4, 32, 2, 4, 60, true, false, false, false, 3, 9},  // Swedish fish #76
        };
        int cells[BOTS], n = 0;
        for (int j = 0; j < BOTS; ++j) {
            const int c = ecell[j];
            bool seen = false;
            for (int k = 0; k < n; ++k) seen |= cells[k] == c;
            if (!seen) cells[n++] = c;
        }
        for (const Sw& s : kSw) {
            if (s.n != n) continue;
            bool all = true;
            for (int k = 0; k < n && all; ++k) {
                bool hit = false;
                for (int q = 0; q < n; ++q) hit |= cells[q] == s.cells[k];
                all = hit;
            }
            if (!all) continue;
            kScanMin = s.lo, kScanMax = s.hi, kRest = s.rest, kLockGrace = s.grace, kShyTurns = s.shy;
            kChain = s.chain, lock_live = s.live, lock_adapt = s.adapt, cb_taxi = s.taxi, kTaxiSize = s.ts, kTaxiFar = s.far;
            kKeep = kMovesCost + kScanMin * kScanCost;
            break;
        }
    }
    for (int i = 0; i < BOTS; ++i) {
        const int x = phais::get_bot_x(0, i), y = phais::get_bot_y(0, i);
        const int c = x < 0 || y < 0 || x >= N || y >= N ? -1 : y * N + x;
        const int from = drag_from[i];
        if (from >= 0 && from != c && C[from] > 0) set_cows(from, C[from] - 1);
        if (plain[i] && from >= 0 && from == c) shy_cell[i] = c, shy_until[i] = turn_no + kShyTurns;
        plain[i] = false;
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

    // Did last turn's locks hold? Held: we are still there and no cow left.
    for (int i = 0; i < BOTS; ++i) {
        if (lock_cell[i] < 0) continue;
        if (at[i] == lock_cell[i] && C[at[i]] >= lock_cows[i]) ++lock_ok;
        else ++lock_bad;
        lock_cell[i] = -1;
    }

    // Lock: an enemy botlet stands with one of ours on a cell with cows. It will
    // drag one. Guess the drag from our own table turned half round, and pull
    // the same cow the other way: the pulls cancel and the cow stays.
    int locks_now = 0;
    if (map_id >= 0 && kLockOn[map_id] && lock_live && turn_no >= loose_until && (!(kLockAdapt[map_id] && lock_adapt) || lock_bad <= lock_ok + kLockGrace) &&
        spent + kLockCost + kKeep < kBudget) {
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
            lock_cell[i] = c, lock_cows[i] = C[c];
            done[i] = true;
            ++locks_now;
        }
    }
    if (map_id == 2) {
        frozen_run = locks_now >= kFrozenLocks ? frozen_run + 1 : 0;
        if (frozen_run == kFrozenCheck || frozen_run == 2 * kFrozenCheck) {
            const int us = phais::get_score(0), them = phais::get_score(1);
            spent += 2 * kRead;
            if (frozen_run == 2 * kFrozenCheck) {
                const int gain = (us - frozen_us) - (them - frozen_them);  // over kFrozenCheck turns
                if ((us - them) * kFrozenCheck + gain * (max_turns - turn_no) < 0) loose_until = turn_no + kLooseTurns;
                frozen_run = 0;
            }
            frozen_us = us, frozen_them = them;
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
            int way = map_id == 0 && kChain ? chain_way(c) : WAY[c], idx = -1;
            if (map_id == 0 && kChain) spent += 400;  // the look-ahead: uncounted, it ran a turn out of fuel
            if (shy_cell[i] == c && turn_no < shy_until[i]) {
                // Our last drag here was cancelled. Take the last cow, or another way out, or leave.
                if (C[c] - taken(c) >= 2) idx = take_last(c);
                else if ((way = other_way(c)) != STAY) idx = take(c);
            } else {
                idx = take(c);
            }
            if (idx >= 0) {
                drop_target(i);
                mv_way[i] = way, mv_drag[i] = idx;
                drag_from[i] = c;
                plain[i] = true;
                done[i] = true;
                continue;
            }
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
    if (lead >= 0 && nf >= kTaxiSize && map_id >= 0 && (kTaxiOn[map_id] || cb_taxi) && !taxi_off && spent + rally_cost + kMovesCost < kBudget) {
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
    for (int i = 0; i < BOTS; ++i) tgt[i] = drag_from[i] = ride[i] = ride_at[i] = shy_cell[i] = lock_cell[i] = -1;
    for (;;) {
        turn_no = phais::get_turn();
        spent = kRead;
        if (map_id == -2) identify(), max_turns = phais::get_max_turns(), spent += 6000;  // the table copies are dear. Kept at 6000 so turn 0 plays as before; no cut-off on any of the three maps
        if (map_id >= 0 || !learn()) play();
        phais::end_turn();
    }
}
