#pragma once
// A vector of at most N items with no heap. push() on a full one drops the
// item: a short list loses less than a crash.

namespace kit {

template <class T, int N>
struct SVec {
    T a[N]{};
    int n = 0;

    bool push(const T& v) {
        if (n == N) return false;
        a[n++] = v;
        return true;
    }
    void pop() { if (n > 0) --n; }
    void clear() { n = 0; }
    // Removes item i by moving the last one into its place: order is not kept.
    void drop(int i) { a[i] = a[--n]; }

    int size() const { return n; }
    bool empty() const { return n == 0; }
    bool full() const { return n == N; }

    T& operator[](int i) { return a[i]; }
    const T& operator[](int i) const { return a[i]; }
    T& back() { return a[n - 1]; }
    const T& back() const { return a[n - 1]; }
    T* begin() { return a; }
    T* end() { return a + n; }
    const T* begin() const { return a; }
    const T* end() const { return a + n; }
};

}
