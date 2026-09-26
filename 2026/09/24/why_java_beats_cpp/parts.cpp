// Decomposes the cost of buf[i & 1023] = std::to_string(i).
#include <charconv>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

std::vector<std::string> buf(1024);
char cbuf[1024][16];

// 1. the benchmark as published
__attribute__((noinline)) void full(uint64_t n) {
  for (uint64_t i = 0; i < n; i++) buf[i & 1023] = std::to_string(i);
}
// 2. to_chars (no stack copy of the digit table), then assign() into the SSO slot
__attribute__((noinline)) void tochars_assign(uint64_t n) {
  char tmp[24];
  for (uint64_t i = 0; i < n; i++) {
    auto r = std::to_chars(tmp, tmp + sizeof tmp, i);
    buf[i & 1023].assign(tmp, r.ptr - tmp);
  }
}
// 3. conversion only: to_chars straight into a plain char ring buffer
__attribute__((noinline)) void tochars_only(uint64_t n) {
  for (uint64_t i = 0; i < n; i++) {
    char *d = cbuf[i & 1023];
    auto r = std::to_chars(d, d + 15, i);
    *r.ptr = 0;
  }
}
// 4. to_string into a temporary that is only observed, not stored
__attribute__((noinline)) void to_string_only(uint64_t n) {
  for (uint64_t i = 0; i < n; i++) {
    std::string s = std::to_string(i);
    asm volatile("" : : "r"(s.data()) : "memory");
  }
}
// 5. store only: move-assign an already-built SSO string (8 chars) into the slot
__attribute__((noinline)) void store_only(uint64_t n) {
  for (uint64_t i = 0; i < n; i++) {
    std::string s(8, char('0' + (i & 7)));
    asm volatile("" : : "r"(s.data()) : "memory");
    buf[i & 1023] = std::move(s);
  }
}

template <class F> void bench(const char *name, F f) {
  const uint64_t N = 100'000'000;
  f(1'000'000);
  double best = 1e300;
  for (int r = 0; r < 5; r++) {
    auto t0 = std::chrono::steady_clock::now();
    f(N);
    auto t1 = std::chrono::steady_clock::now();
    double dt = std::chrono::duration<double, std::nano>(t1 - t0).count();
    if (dt < best) best = dt;
  }
  std::printf("%-28s %6.2f ns/iter\n", name, best / N);
}

int main(int argc, char **argv) {
  if (argc > 1) {  // single-variant mode for valgrind: parts <k> <n>
    uint64_t n = argc > 2 ? strtoull(argv[2], 0, 10) : 10'000'000;
    void (*fs[])(uint64_t) = {full, tochars_assign, tochars_only, to_string_only, store_only};
    fs[atoi(argv[1])](n);
    return 0;
  }
  bench("1 full (to_string + move)", full);
  bench("2 to_chars + assign", tochars_assign);
  bench("3 to_chars only", tochars_only);
  bench("4 to_string only", to_string_only);
  bench("5 move-assign only", store_only);
}
