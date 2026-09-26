// Same benchmark, but the digits are written directly into buf[k]'s own SSO storage:
// no temporary std::string, no move-assign, no destructor.
#include <charconv>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>
std::vector<std::string> buf(1024);
__attribute__((noinline)) void direct(uint64_t n) {
  for (uint64_t i = 0; i < n; i++)
    buf[i & 1023].resize_and_overwrite(20, [i](char *p, size_t m) {
      return size_t(std::to_chars(p, p + m, i).ptr - p);
    });
}
__attribute__((noinline)) void full(uint64_t n) {
  for (uint64_t i = 0; i < n; i++) buf[i & 1023] = std::to_string(i);
}
template <class F> void bench(const char *name, F f) {
  const uint64_t N = 100'000'000; f(1'000'000); double best = 1e300;
  for (int r = 0; r < 5; r++) {
    auto t0 = std::chrono::steady_clock::now(); f(N);
    double dt = std::chrono::duration<double, std::nano>(std::chrono::steady_clock::now() - t0).count();
    if (dt < best) best = dt;
  }
  size_t t = 0; for (auto &s : buf) t += s.size();
  std::printf("%-34s %6.2f ns/string (check %zu)\n", name, best / N, t);
}
int main() {
  bench("buf[k] = to_string(i)", full);
  bench("buf[k].resize_and_overwrite(to_chars)", direct);
}
