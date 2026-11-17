#pragma once
// Which job next, without two units taking the same one. From cambc's planner,
// radio and stall rules.
#include <algorithm>
#include <cstdint>
#include <span>
#include <vector>
#include "../kit/grid.h"

namespace kitcambc {

// Team notes in shared bytes. Byte s holds unit s's job (6 bits) and status
// (2 bits). Three more bytes hold one tile with a round stamp.
// Each turn: copy the game's shared bytes into `bytes`, and write back the
// bytes marked in `dirty`.
struct Radio {
    static constexpr int kSlots = 6, kMaxJob = 62, kSize = kSlots + 3;
    enum Status { travelling = 0, working = 1, finishing = 2, waiting = 3 };
    struct Intent {
        int job = -1;
        Status status = travelling;
    };

    std::uint8_t bytes[kSize] = {};
    bool dirty[kSize] = {};

    Intent intent(int slot) const {
        if (slot < 0 || slot >= kSlots) return {};
        return {(bytes[slot] & 63) - 1, Status(bytes[slot] >> 6)};
    }
    void set_intent(int slot, int job, Status status) {
        if (slot < 0 || slot >= kSlots) return;
        const int j = job < 0 || job > kMaxJob ? 0 : job + 1;
        put(slot, std::uint8_t(j | (status << 6)));
    }

    // One tile for the team, such as where the fight is. x and y up to 127.
    void post(kit::P at, int round) {
        if (at.x < 0 || at.y < 0 || at.x > 127 || at.y > 127) return;
        put(kSlots, std::uint8_t(at.x | 128));
        put(kSlots + 1, std::uint8_t(at.y));
        put(kSlots + 2, std::uint8_t(round & 255));
    }
    // The tile, if it was posted in the last `fresh` rounds.
    bool read(int round, kit::P& at, int fresh = 2) const {
        if (!(bytes[kSlots] & 128) || ((round - bytes[kSlots + 2]) & 255) > fresh) return false;
        at = {bytes[kSlots] & 127, bytes[kSlots + 1] & 127};
        return true;
    }

  private:
    void put(int i, std::uint8_t b) {
        if (bytes[i] == b) return;
        bytes[i] = b, dirty[i] = true;
    }
};

// Jobs given up for a while. A job that makes no progress costs the game.
struct Bans {
    std::vector<int> until;  // per job: banned before this round

    void ban(int job, int until_round) {
        if (job < 0) return;
        if (job >= int(until.size())) until.resize(job + 1, 0);
        until[job] = std::max(until[job], until_round);
    }
    bool banned(int job, int round) const { return job >= 0 && job < int(until.size()) && round < until[job]; }

    // Call each turn spent on `job`. `left` must fall as the work gets done
    // (enemy health, steps to go). After `patience` turns with no fall the job
    // is banned for `rounds`. True when it has just been banned.
    bool watch(int job, float left, int round, int patience = 12, int rounds = 60) {
        if (job != job_ || left < best_) job_ = job, best_ = left, still_ = 0;
        else if (++still_ >= patience) {
            ban(job, round + rounds);
            job_ = -1;
            return true;
        }
        return false;
    }

  private:
    int job_ = -1, still_ = 0;
    float best_ = 0;
};

// Reward for each turn spent: walk there, wait for it, do it.
inline double rate(double value, int walk, int wait, int work) { return value / std::max(1, walk + wait + work); }

// Whether `me` leaves a job to `other`, who wants it too. The one already
// working keeps it. Otherwise the lower slot does.
inline bool yields(int me, int other, bool other_working) { return other_working || other < me; }

// The job with the best rate that is not banned and not left to a teammate.
// rate_of(job) -> double; zero or less means do not take it. -1 when none.
template <class RateOf>
int choose(int jobs, int me, const Radio& radio, const Bans& bans, int round, RateOf&& rate_of) {
    int best = -1;
    double best_r = 0;
    for (int j = 0; j < jobs; ++j) {
        if (bans.banned(j, round)) continue;
        bool taken = false;
        for (int s = 0; s < Radio::kSlots && !taken; ++s) {
            const auto it = radio.intent(s);
            const bool busy = it.status == Radio::working || it.status == Radio::finishing;
            taken = s != me && it.job == j && yields(me, s, busy);
        }
        if (taken) continue;
        const double r = rate_of(j);
        if (r > best_r) best_r = r, best = j;
    }
    return best;
}

}
