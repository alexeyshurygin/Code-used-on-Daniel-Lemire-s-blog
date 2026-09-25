# Java 27 version of the string-creation benchmark

`Bench.java` is a Java port of the benchmark in `post.md`: it converts the integers
0 to 100 million into strings, storing each one in a 1024-slot ring buffer, and
reports the best of five runs after a 1-million-iteration warm-up. The measured
function is:

```java
static void fromInt(int n) {
  final String[] b = buf;
  for (int i = 0; i < n; i++) b[i & 1023] = Integer.toString(i);
}
```

It also times `"" + i` (string concatenation), which runs at the same speed.

## Machine and versions

A cloud VM with 4 vCPUs of an Intel Xeon @ 2.1 GHz, running Ubuntu 24.04. Runs were not
pinned to a core, the same as the M4 Max runs.

OpenJDK 27+35 (GA, default G1 GC), CPython 3.14.7, Node.js 25.9.0, Bun 1.4.2,
Ubuntu clang 18.1.3 with libstdc++, rustc 1.94.1 with itoa, Go 1.24.7, Nim 2.2.12.
For comparison: OpenJDK 21.0.10 and Temurin 25.0.4.1.

Reproduce with `./run_all.sh` (set `JAVA_HOME` to a JDK 27 install and `PYTHON` to a 3.14
interpreter). Plot with `python plot.py results_linux_xeon.txt strings_linux_java27.png "<subtitle>"`
and `python plot_java.py`.

## Results (ns per string, lower is better)

| Language / call          | Xeon (this run) | Apple M4 Max (post) |
|--------------------------|----------------:|--------------------:|
| **Java 27 `Integer.toString`** | **10.8** | n/a |
| C++ `std::to_string`     | 12.1 | 5.4  |
| Nim `$i`                 | 16.4 | 11.7 |
| Rust `itoa`              | 17.6 | 14.1 |
| Rust `to_string()`       | 20.9 | 15.7 |
| Go `strconv.Itoa`        | 21.7 | 11.9 |
| Node.js `String(i)`      | 22.7 | 13.7 |
| Bun `String(i)`          | 49.2 | 14.6 |
| Python `str(i)`          | 58.9 | 43.6 |

![Converting an integer to a new string, Intel Xeon, with Java 27](strings_linux_java27.png)

In two more runs, Java 27 took 10.5 to 10.7 ns and C++ took 12.0 to 12.4 ns, so the
order holds.

### Java across JDK releases (`Integer.toString`, ns per string)

| GC       | JDK 21 | JDK 25 | JDK 27 |
|----------|-------:|-------:|-------:|
| G1 (default) | 16.6 | 14.2 | **10.4** |
| Parallel | 11.6 | 9.8  | 11.0 |
| Serial   | 11.0 | 8.6  | 12.1 |
| ZGC      | 12.2 | 11.1 | 12.1 |

![Java Integer.toString by JDK release](java_versions.png)

## Observations

* **Java 27 is the fastest entry on this machine**, a little ahead of C++. Each Java string is
  a new heap object: a `String` plus a Latin-1 `byte[]`, because compact strings are on by
  default. But HotSpot allocates in a thread-local buffer by bumping a pointer, and the
  young-generation collector frees these short-lived objects almost for free. That makes an
  allocation nearly as cheap as C++'s small-string-optimized `std::string`, which never
  allocates at all.
* **This is a JDK 27 improvement.** With the default G1 collector, the time drops from 16.6 ns
  (JDK 21) to 14.2 ns (JDK 25) to 10.4 ns (JDK 27). Most of the older releases' gap with G1 came
  from G1's write barrier on every reference store into the array. JDK 27 closes that gap, so G1
  now runs as fast as the throughput collectors (Parallel, Serial). The recent G1
  barrier-simplification work in OpenJDK is the likely cause.
* The ranking of the other languages is similar to the M4 Max, except that C++ has a much
  smaller lead. That is probably libstdc++'s `to_string` versus Apple's libc++. Bun's
  `String(i)` also does comparatively worse on this Linux/x64 box.
* The GC rows other than the default vary by about ±1 ns between runs on this shared VM.
  Treat differences that small as noise.
