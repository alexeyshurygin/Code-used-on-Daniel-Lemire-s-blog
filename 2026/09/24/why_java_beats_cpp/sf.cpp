// Store-to-load forwarding test: write 8 digit bytes, then copy them out.
//  narrow_then_wide : 8 separate byte stores, then one 8-byte load (like to_string + move)
//  wide_then_wide   : one 8-byte store, then one 8-byte load
//  narrow_no_read   : 8 byte stores, never read back (like Java's byte[] fill)
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>

alignas(64) char tmp[32];
alignas(64) uint64_t out[1024];
alignas(64) char outc[1024][8];

__attribute__((noinline)) void narrow_then_wide(uint64_t n) {
  for (uint64_t i = 0; i < n; i++) {
    volatile char *t = tmp;
    for (int k = 0; k < 8; k++) t[k] = char('0' + ((i >> k) & 7));
    uint64_t v;
    std::memcpy(&v, tmp, 8);
    out[i & 1023] = v;
  }
}
__attribute__((noinline)) void wide_then_wide(uint64_t n) {
  for (uint64_t i = 0; i < n; i++) {
    uint64_t w = 0;
    for (int k = 0; k < 8; k++) w |= uint64_t('0' + ((i >> k) & 7)) << (8 * k);
    *(volatile uint64_t *)tmp = w;
    uint64_t v;
    std::memcpy(&v, tmp, 8);
    out[i & 1023] = v;
  }
}
__attribute__((noinline)) void narrow_no_read(uint64_t n) {
  for (uint64_t i = 0; i < n; i++) {
    volatile char *t = outc[i & 1023];
    for (int k = 0; k < 8; k++) t[k] = char('0' + ((i >> k) & 7));
  }
}

template <class F> void bench(const char *name, F f) {
  const uint64_t N = 100'000'000;
  f(1'000'000);
  double best = 1e300;
  for (int r = 0; r < 5; r++) {
    auto t0 = std::chrono::steady_clock::now();
    f(N);
    double dt = std::chrono::duration<double, std::nano>(std::chrono::steady_clock::now() - t0).count();
    if (dt < best) best = dt;
  }
  std::printf("%-20s %6.2f ns/iter\n", name, best / N);
}

int main() {
  bench("narrow_then_wide", narrow_then_wide);
  bench("wide_then_wide", wide_then_wide);
  bench("narrow_no_read", narrow_no_read);
}
