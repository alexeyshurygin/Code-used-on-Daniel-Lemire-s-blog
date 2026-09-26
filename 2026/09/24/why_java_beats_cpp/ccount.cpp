// Same loop as bench.cpp over [10M, 10M + K): 8-digit numbers, for valgrind counting.
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
std::vector<std::string> buf(1024);
__attribute__((noinline)) void from_int(uint64_t from, uint64_t n) {
  for (uint64_t i = from; i < from + n; i++) buf[i & 1023] = std::to_string(i);
}
int main(int, char **argv) {
  from_int(10'000'000, strtoull(argv[1], 0, 10));
  size_t t = 0; for (auto &s : buf) t += s.size();
  std::printf("%zu\n", t);
}
