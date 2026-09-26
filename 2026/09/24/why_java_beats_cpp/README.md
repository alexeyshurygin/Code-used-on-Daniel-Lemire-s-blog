# Why Java 27 beats C++ on the string-creation benchmark (Linux, Intel Xeon)

On the test VM, `buf[i & 1023] = Integer.toString(i)` on Java 27 takes about **10.5 ns** per string.
`buf[i & 1023] = std::to_string(i)` in C++ takes about **12.0 ns**. This directory holds the
evidence for why.

**Short answer.** C++ does the digit conversion itself faster than Java. It loses because of
how `std::to_string` delivers the result. The function returns a *temporary* `std::string`. Then
the loop *move-assigns* that temporary into the vector slot. In libstdc++, building the temporary
costs a `memset` call, which zero-fills the buffer before the digits are written. The move costs
a `memcpy` call, because both strings are SSO strings, so their bytes must be copied. The
compiler also emits branches for each SSO-versus-heap case. Java has no temporary, no copy and
no calls. HotSpot inlines the whole `Integer.toString`: the digits are written once, straight
into the new `byte[]`. Java zeroes that array too, but in 6 inline instructions. The two objects
come from a bump-pointer allocation in a thread-local buffer (TLAB). The only other work per
string is a 4-byte compressed-pointer store and G1's JDK 26+ card-mark barrier, together about
18 instructions with no fence and no call. When C++ also
writes the digits straight into the vector slot, it takes **6.0 ns**. That variant doesn't create
a new string, though; it overwrites an existing one in place. So it isn't a fair entry in the
benchmark. It shows where the original C++ loop spends its time (section 4).

The machine: KVM guest, 4 vCPUs of an Intel Xeon (family 6, model 207, Emerald Rapids), measured
at 3.4 GHz. The CPU supports AVX-512. There are no hardware performance counters in the guest.

## 1. The timings hold up (three independent checks)

`timing.sh` runs every variant in turn (`timing_unpinned.txt`, `timing_pinned.txt`). Each launch
reports the best of 5 runs × 100M strings.

| variant | unpinned, 10 launches: min / median / max | pinned to one core (`taskset -c 2`), 5 launches: median |
|---|---|---|
| C++ clang 18 + libstdc++ (the `run_all.sh` build) | 11.73 / 12.06 / 12.72 | 12.01 |
| C++ g++ 13 + libstdc++ | 11.71 / 11.86 / 12.63 | 12.26 |
| C++ clang 18 + libc++ | 13.17 / 13.35 / 13.59 | 13.44 |
| **Java 27, `long` elapsed time** | **10.42 / 10.81 / 11.41** | **10.31** |
| Java 27, `double` elapsed time (original) | 10.23 / 10.56 / 11.45 | 10.17 |
| Java 25, G1 | 13.90 / 14.16 / 14.54 | 15.68 |

* Java 27 is ahead in every launch. Its worst launch (11.41 ns) is about as fast as C++'s best
  launch (11.73 ns).
* Timing with a `long` or a `double` makes no difference. The ~0.2 ns gap is noise. The run
  takes about 10⁹ ns, which a double represents exactly, and the difference is computed as a
  `long` before any conversion. `Bench.java` now uses `long`.
* Pinning to one core doesn't change the result. With a single CPU, GC threads and JIT
  compiler threads share the core with the benchmark and count in its wall time. So Java isn't
  winning by pushing work onto other cores.

## 2. The machine code: same algorithm, different packaging

Both libraries use the same textbook algorithm. They count the digits with a chain of compares,
then emit two digits per step with a divide-by-100 (done as a multiply) and a 200-entry table
lookup. The difference is what surrounds it. The listings are `asm_*.txt`.

**C++, clang + libstdc++ (`asm_cpp_clang.txt`)**, hot path for an 8-digit number:

1. `std::string __str(len, '\0')` → **`call memset@plt`** (at `13b0`) zero-fills the 8 SSO bytes.
2. The digit loop writes 2 separate bytes per pair.
3. `buf[k] = std::move(tmp)`: the destination is SSO and the source is SSO, so the chars must be
   copied → **`call memcpy@plt`** (at `1514`). The code also updates the length, writes the NUL,
   resets the temporary, and runs the destructor's "is it on the heap?" branch.

**C++, g++ (`asm_cpp_gcc.txt`)**: libstdc++ 13 declares the digit table as a *non-static*
`constexpr char __digits[201]` (see `bits/charconv.h`). GCC rebuilds that table on the stack on
*every call*: 13 × 16-byte SSE loads and stores (26 instructions, `195b`–`1a27`). The `memset` is
still called. The 8-byte copy is inlined. clang promotes the table to a global.

**C++, clang + libc++ (`asm_cpp_libcxx.txt`)**: `to_string` isn't inlined. Each string costs a PLT
call to `std::to_string` in `libc++.so`, a second call to its `__u32toa` helper, and a `memcpy`
call. Then the caller copies the 24-byte string representation.

**Java 27 (`asm_java27_g1.txt`)**: the whole `Integer.toString` is inlined into the loop by C2,
with no calls on the fast path:

* allocation of the `byte[]`: TLAB bump (`add`, `cmp` against the end, store the new top), two
  header stores, and zeroing 8 bytes;
* digits: two per iteration, each pair written with **one 16-bit store**
  (`mov WORD PTR [rdx+r14*1+0xc],r8w`), because C2 merged the two byte stores;
* allocation of the `String`: another TLAB bump plus header stores; the `byte[]` pointer is
  stored in it, and nothing is copied;
* the store into `buf`: a 4-byte compressed pointer, then G1's post-barrier: a cross-region test,
  then `cmp BYTE PTR [card],0xff` / `mov BYTE PTR [card],0x0`.

## 3. Instruction counts (valgrind, exact) and cycles

Valgrind (callgrind) counted the C++ instructions per 8-digit string, taking the difference
between two run lengths to cancel startup costs (`ccount.cpp`). The Java number comes from
valgrind over the whole JVM process (`JCount.java`) together with a hand count of the C2 hot path.
See the caveats. The clock is 3.4 GHz,
from a dependent-`imul` chain (`freq2.c`). A dependent-`add` chain reads an impossible 16 GHz on
this core, because it executes those adds at rename.

| build | instructions / string | libc or library calls / string | ns | cycles | IPC |
|---|---:|---|---:|---:|---:|
| **Java 27** | **~170–178** | 0 | 10.5 | 36 | 4.7–4.9 |
| C++ clang + libstdc++ | 174 | `memset`, `memcpy` | 12.0 | 41 | 4.3 |
| C++ g++ + libstdc++ | 230 | `memset` | 11.9 | 40 | 5.7 |
| C++ clang + libc++ | 123 | `to_string`, `__u32toa`, `memcpy` | 13.4 | 46 | 2.7 |

callgrind's per-function profile confirms the calls: exactly one `__memset_avx2_unaligned_erms`
and one `__memcpy_avx_unaligned_erms` per string for clang + libstdc++, 13 instructions each.

The instruction count alone doesn't decide it. libc++ runs the fewest instructions and is the
slowest, at an IPC of 2.7. Its work is spread across three calls and returns, with stack traffic
between them. Java executes about as many instructions as clang + libstdc++. They are one
straight-line block with no call or return, and it runs 5 cycles faster.

## 4. Isolating each cost

`direct.cpp` ends with the same `std::vector<std::string>` contents. But instead of building a new
string, it writes the digits straight into the existing slot's SSO buffer with C++23
`resize_and_overwrite` + `std::to_chars`. That removes the temporary, the zero-fill and the move:

```cpp
// original: build a temporary, then move-assign it into the slot
buf[i & 1023] = std::to_string(i);

// direct: overwrite the slot's own characters in place
buf[i & 1023].resize_and_overwrite(20, [i](char *p, size_t m) {
  return size_t(std::to_chars(p, p + m, i).ptr - p);
});
```

| build | `buf[k] = to_string(i)` | direct into `buf[k]` |
|---|---:|---:|
| g++ + libstdc++ | 13.65 | 7.50 |
| clang + libstdc++ | 10.86 | **6.00** |
| clang + libc++ | 13.14 | 6.62 |

About half of C++'s time goes to the temporary-and-move, not to formatting digits.

**This is a diagnostic, not a fair entry in the benchmark.** The direct variant creates no new
string; it reuses storage that already exists. The original C++ loop and Java's
`Integer.toString` both produce a brand-new string every time, and the benchmark measures that.
Compare like with like:

| work per number | C++ (clang + libstdc++) | Java 27 | faster |
|---|---:|---:|---|
| conversion only, into existing storage | 6.0 (`direct.cpp`) | ~8.7 (`JavaParts.java` #2) | C++ |
| a new string each time (the benchmark) | 12.0 (`bench.cpp`) | 10.5 (`Bench.java`) | **Java** |
| cost of making it a *new* string | ~5–6 | ~2 | Java |

The Java conversion-only figure is approximate. It uses the same JDK digit code
(`DecimalDigits`), but it also pays an extra array-of-arrays lookup per string. C++ formats the
digits faster, but its way of producing a new string (temporary + `memset` + `memcpy` + destructor
checks) costs about three times Java's way (bump allocation + one reference store). That's why
Java wins the benchmark as defined. Some of Java's ~2 ns overlaps with the digit math under
out-of-order execution.

(`parts.cpp` has a finer split. The parts overlap under out-of-order execution, so their times
don't add up exactly: clang + libstdc++ `to_chars` alone takes 5.1 ns, `to_string` into an unused
temporary 6.8 ns, and move-assigning an 8-char SSO string 4.0 ns.)

Store-to-load forwarding was a candidate too: digits are stored one byte at a time and then read
back with one wide load. `sf.cpp` tests this and finds no measurable penalty on this core (7.9 ns
for narrow stores then a wide load, vs 8.5 ns for a wide store then a wide load). The iterations
are independent, so out-of-order execution hides the latency. It isn't the explanation.

## 5. Allocation and GC are nearly free here

`GcCost.java` measures each 100M-string run. It records wall time, the process CPU time summed
over all threads, and the GC count and time:

| setup | ns/string (wall) | ns/string (CPU, all threads) | young GCs per run | GC time |
|---|---:|---:|---:|---:|
| JDK 27, default (G1, 4 CPUs) | 10.1–10.7 | 10.2–10.8 | 9–11 | 11–13 ms (≈1.2%) |
| JDK 27, pinned to 1 core | 10.4–10.7 | 10.5–10.7 | 9–11 | 5–10 ms (≈0.7%) |
| JDK 27, `-Xmn4g` (huge young gen) | 10.2–11.1 | 10.1–11.1 | 1 | 2–4 ms (≈0.3%) |

A full `Bench` run (`-Xlog:gc`) has 119 young pauses, averaging 1.24 ms, 147 ms in total over
about 11 s of work. Each run allocates 4.8 GB (48 bytes per string: a 24-byte `String` plus a
24-byte `byte[8]`, see `../AllocSize.java`). But only the 1024 strings in `buf` are alive at any
moment, and G1's young collections only touch live objects, so they're nearly free. CPU time
equals wall time, so no background thread does hidden work in parallel. Cutting the number of GCs
from ~10 to 1 per run doesn't change the speed. Allocation itself is inline code: about 25
instructions for the `byte[]` and 16 for the `String`, including `prefetchw` hints that pull the
next TLAB lines into cache ahead of use.

## 6. Why the same Java code is much slower on JDK 25

`asm_java25_g1.txt` is the JDK 25 G1 code for the same loop. Storing a young `String` into the
old-generation `buf` array fails the "young card" test and jumps to an out-of-line path that
starts with

```
lock add DWORD PTR [rsp-0x40],0x0     ; full StoreLoad fence
cmp    BYTE PTR [card],0x0            ; already dirty?
```

That is a **full memory fence on every string**. The fence makes the core drain its store buffer
before continuing. [JEP 522](https://openjdk.org/jeps/522), in JDK 26, redesigned the G1
post-barrier: application threads now mark cards in their own card table without
synchronization. The JDK 26/27 code is just `cmp BYTE PTR [card],0xff` / `mov BYTE PTR [card],0x0`,
with no fence. The cost fits the numbers: JDK 25 G1 takes 14.1 ns, while JDK 25 Parallel, whose
barrier has no fence, takes 9.8 ns. The difference is ≈ 4.3 ns ≈ 15 cycles per string.
JDK 26 already runs at 10.4–10.6 ns (see `../results_linux_xeon_g1_matrix.txt`). Nothing in
JDK 27 itself changes this benchmark: compact object headers (JEP 534) don't shrink these
objects, and "G1 everywhere" (JEP 523) doesn't apply on a 4-CPU machine.

## Caveats

* This is one Linux VM. On the Apple M4 Max in the post, C++ with libc++ takes 5.4 ns. That is
  much faster than anything here, so this result is specific to this CPU and these libraries.
* Out-of-line calls and code layout matter. The same C++ `to_string` loop ranged from 10.9 to
  13.7 ns depending on which binary it was compiled into.
* The Java instruction count is less certain than the C++ one. One valgrind pair gave 168
  instructions per string. A second pair gave 335, because its 100M run was a 7.5-G-instruction
  outlier: JIT compilation and deoptimization happen at different times under valgrind's 50×
  slowdown, so valgrind is unreliable for counting instructions in JIT code. A hand trace of the
  8-digit path through `asm_java27_g1.txt` gives ~178 instructions. The table uses 170–178. There
  are no hardware counters in this VM to settle it.

## Reproducing

`timing.sh` expects `cpp_clang`, `cpp_gcc` and `cpp_libcxx` (built from `../bench.cpp` with
clang++ -O3, g++ -O3, and clang++ -O3 -stdlib=libc++), and `jlong/` and `jdbl/` class directories
(`Bench.java` with `long` and `double` timing). Java listings: install `hsdis-amd64.so` in
`$JAVA_HOME/lib/server` and run
`java -XX:+UnlockDiagnosticVMOptions -XX:CompileCommand=print,Bench::fromInt -XX:PrintAssemblyOptions=intel -cp . Bench`.
`JavaParts` and `JCount` need `--add-exports java.base/jdk.internal.util=ALL-UNNAMED` to compile
and run.
