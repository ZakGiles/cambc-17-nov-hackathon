#pragma once
// xoshiro256**: fast, seedable, the same sequence on every platform.
#include <cstdint>
#include <utility>

namespace kit {

struct Rng {
    std::uint64_t s[4];

    explicit Rng(std::uint64_t seed = 1) {
        for (auto& x : s) {
            seed += 0x9E3779B97F4A7C15ull;
            std::uint64_t z = seed;
            z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
            z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
            x = z ^ (z >> 31);
        }
    }

    std::uint64_t next() {
        const std::uint64_t r = rotl(s[1] * 5, 7) * 9, t = s[1] << 17;
        s[2] ^= s[0], s[3] ^= s[1], s[1] ^= s[2], s[0] ^= s[3];
        s[2] ^= t;
        s[3] = rotl(s[3], 45);
        return r;
    }

    // 0 .. n-1, for n > 0.
    int below(int n) { return int(((next() >> 32) * std::uint64_t(n)) >> 32); }
    // lo .. hi, both included.
    int range(int lo, int hi) { return lo + below(hi - lo + 1); }
    // 0 <= x < 1.
    double unit() { return double(next() >> 11) * (1.0 / 9007199254740992.0); }
    bool chance(double p) { return unit() < p; }

    template <class C>
    void shuffle(C& c) {
        for (int i = int(c.size()) - 1; i > 0; --i) std::swap(c[i], c[below(i + 1)]);
    }
    template <class C>
    auto& pick(C& c) { return c[below(int(c.size()))]; }

  private:
    static std::uint64_t rotl(std::uint64_t x, int k) { return (x << k) | (x >> (64 - k)); }
};

}
