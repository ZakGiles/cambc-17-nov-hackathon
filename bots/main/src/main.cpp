// Greedy cow herder. One program moves all 8 botlets.
//
// Fuel: an API call costs about 100, a loop step about 5-10. A turn has 20,000.
// So: read the whole board once at the start (spills over 2-3 turns, which is
// fine: state stays in memory). After that, re-read only cells where bots are
// or just were. Cows only move with bots, so that sees every change.
//
// Each cell gets a static potential P: value * 100 minus distance to the
// nearest best cell. A bot on a cow drags it uphill on P. Otherwise it walks
// to a cow it can improve, and keeps that target across turns.
#include "phais.hpp"

namespace {

constexpr int MAXN = 32;
constexpr int BOTS = 8;
constexpr int ALL = 2 * BOTS;
constexpr int K = 100;
constexpr int REPLAN_PER_TURN = 3;
constexpr int REFRESH_PER_TURN = 4;

int W = 0, H = 0;
int val[MAXN][MAXN];     // value from our side
int pot[MAXN][MAXN];     // potential P
int gain[MAXN][MAXN];    // best one-step P gain from this cell
int gdir[MAXN][MAXN];    // direction of that gain, -1 if none
int cows[MAXN][MAXN];
int claim[MAXN][MAXN];   // bots heading to this cell
int used[MAXN][MAXN];    // drag indexes used this turn on this cell
int lead[MAXN][MAXN];    // direction of the first drag on this cell this turn
int seen[MAXN][MAXN];    // turn stamp: cell already re-read this turn

int px[ALL], py[ALL];    // all 16 bots' positions last turn
int tx[BOTS], ty[BOTS];  // our bots' targets, -1 if none
int refresh_at = 0;
int stamp = 0;

const int DX[4] = {0, 1, 0, -1};  // N E S W
const int DY[4] = {1, 0, -1, 0};

bool inb(int x, int y) { return x >= 0 && y >= 0 && x < W && y < H; }
int iabs(int a) { return a < 0 ? -a : a; }

void read_cell(int x, int y) {
    if (!inb(x, y) || seen[x][y] == stamp) return;
    seen[x][y] = stamp;
    int c = phais::get_cows(x, y);
    cows[x][y] = c < 0 ? 0 : c;
}

void setup() {
    W = phais::get_width();
    H = phais::get_height();
    if (W > MAXN) W = MAXN;
    if (H > MAXN) H = MAXN;
    if (W < 1) W = 1;
    if (H < 1) H = 1;

    // Our corner is (0,0). If the values there are negative, flip.
    int near = 0, far = 0;
    for (int x = 0; x < W; ++x)
        for (int y = 0; y < H; ++y) {
            int v = phais::get_productivity(x, y);
            val[x][y] = v;
            if (x + y < (W + H) / 2) near += v; else far += v;
            cows[x][y] = 0;
            seen[x][y] = -1;
        }
    int sign = near < far ? -1 : 1;

    int best = -1000;
    for (int x = 0; x < W; ++x)
        for (int y = 0; y < H; ++y) {
            val[x][y] *= sign;
            if (val[x][y] > best) best = val[x][y];
        }

    // BFS distance to the nearest best cell.
    static int dist[MAXN][MAXN];
    static int qx[MAXN * MAXN], qy[MAXN * MAXN];
    int head = 0, tail = 0;
    for (int x = 0; x < W; ++x)
        for (int y = 0; y < H; ++y) {
            dist[x][y] = -1;
            if (val[x][y] == best) {
                dist[x][y] = 0;
                qx[tail] = x; qy[tail] = y; ++tail;
            }
        }
    while (head < tail) {
        int x = qx[head], y = qy[head]; ++head;
        for (int d = 0; d < 4; ++d) {
            int nx = x + DX[d], ny = y + DY[d];
            if (inb(nx, ny) && dist[nx][ny] < 0) {
                dist[nx][ny] = dist[x][y] + 1;
                qx[tail] = nx; qy[tail] = ny; ++tail;
            }
        }
    }
    for (int x = 0; x < W; ++x)
        for (int y = 0; y < H; ++y)
            pot[x][y] = val[x][y] * K - (dist[x][y] < 0 ? 0 : dist[x][y]);

    for (int x = 0; x < W; ++x)
        for (int y = 0; y < H; ++y) {
            gain[x][y] = 0;
            gdir[x][y] = -1;
            for (int d = 0; d < 4; ++d) {
                int nx = x + DX[d], ny = y + DY[d];
                if (!inb(nx, ny)) continue;
                int g = pot[nx][ny] - pot[x][y];
                if (g > gain[x][y]) { gain[x][y] = g; gdir[x][y] = d; }
            }
        }

    // Full cow read, once.
    for (int x = 0; x < W; ++x)
        for (int y = 0; y < H; ++y) {
            int c = phais::get_cows(x, y);
            cows[x][y] = c < 0 ? 0 : c;
        }

    for (int i = 0; i < ALL; ++i) { px[i] = -1; py[i] = -1; }
    for (int i = 0; i < BOTS; ++i) { tx[i] = -1; ty[i] = -1; }
}

// One step from (x,y) toward (gx,gy). Prefer the longer axis.
int step_to(int x, int y, int gx, int gy) {
    int ax = gx - x, ay = gy - y;
    if (ax == 0 && ay == 0) return -1;
    if (iabs(ax) >= iabs(ay)) return ax > 0 ? 1 : 3;
    return ay > 0 ? 0 : 2;
}

bool useful(int x, int y) { return inb(x, y) && cows[x][y] > 0 && gain[x][y] > 0; }

void turn() {
    ++stamp;

    // Bot positions: ours 0..7, theirs 8..15.
    int bx[ALL], by[ALL];
    for (int i = 0; i < ALL; ++i) {
        bx[i] = phais::get_bot_x(i < BOTS ? 0 : 1, i % BOTS);
        by[i] = phais::get_bot_y(i < BOTS ? 0 : 1, i % BOTS);
    }

    // Re-read cows where any bot is or was.
    for (int i = 0; i < ALL; ++i) {
        read_cell(bx[i], by[i]);
        read_cell(px[i], py[i]);
        px[i] = bx[i];
        py[i] = by[i];
    }
    // Slow sweep to fix anything missed.
    for (int k = 0; k < REFRESH_PER_TURN; ++k) {
        int n = refresh_at++ % (W * H);
        read_cell(n % W, n / W);
    }

    // One pass: reset per-turn counts and list the cows worth moving.
    static int lx[MAXN * MAXN], ly[MAXN * MAXN];
    int nl = 0;
    for (int x = 0; x < W; ++x)
        for (int y = 0; y < H; ++y) {
            claim[x][y] = 0;
            used[x][y] = 0;
            if (cows[x][y] > 0 && gain[x][y] > 0) { lx[nl] = x; ly[nl] = y; ++nl; }
        }

    bool done[BOTS];
    for (int i = 0; i < BOTS; ++i) done[i] = !inb(bx[i], by[i]);

    // 1. Drag: a bot on a cow that can go uphill drags it.
    //    Extra bots on the cell stack on cow 0 if a second step still helps.
    for (int i = 0; i < BOTS; ++i) {
        if (done[i]) continue;
        int x = bx[i], y = by[i];
        if (!useful(x, y)) continue;
        if (used[x][y] < cows[x][y]) {
            int d = gdir[x][y];
            if (used[x][y] == 0) lead[x][y] = d;
            phais::submit_move(i, static_cast<phais::Direction>(d), used[x][y]);
            ++used[x][y];
            done[i] = true;
        } else {
            int d = lead[x][y];
            int mx = x + DX[d], my = y + DY[d];
            int nx = mx + DX[d], ny = my + DY[d];
            if (inb(nx, ny) && pot[nx][ny] > pot[mx][my]) {
                phais::submit_move(i, static_cast<phais::Direction>(d), 0);
                done[i] = true;
            }
        }
        if (done[i]) { tx[i] = -1; ty[i] = -1; }
    }

    // 2. Keep valid targets and count claims.
    for (int i = 0; i < BOTS; ++i) {
        if (done[i]) continue;
        if (tx[i] >= 0 && useful(tx[i], ty[i]) && claim[tx[i]][ty[i]] < cows[tx[i]][ty[i]])
            ++claim[tx[i]][ty[i]];
        else
            tx[i] = ty[i] = -1;
    }

    // 3. Replan a few bots with no target. Best gain / (distance + 2).
    int replans = 0;
    for (int i = 0; i < BOTS && replans < REPLAN_PER_TURN; ++i) {
        if (done[i] || tx[i] >= 0) continue;
        ++replans;
        int x = bx[i], y = by[i];
        int bg = 0, bd = 1, gx = -1, gy = -1;
        for (int k = 0; k < nl; ++k) {
            int cx = lx[k], cy = ly[k];
            if (cows[cx][cy] <= claim[cx][cy]) continue;
            int g = gain[cx][cy];
            int d = iabs(cx - x) + iabs(cy - y) + 2;
            if (g * bd > bg * d) { bg = g; bd = d; gx = cx; gy = cy; }
        }
        if (gx >= 0) { tx[i] = gx; ty[i] = gy; ++claim[gx][gy]; }
    }

    // 4. Walk to targets.
    for (int i = 0; i < BOTS; ++i) {
        if (done[i] || tx[i] < 0) continue;
        int d = step_to(bx[i], by[i], tx[i], ty[i]);
        if (d >= 0) phais::submit_move(i, static_cast<phais::Direction>(d), -1);
    }
}

}  // namespace

int main() {
    setup();
    for (;;) {
        turn();
        phais::end_turn();
    }
}
