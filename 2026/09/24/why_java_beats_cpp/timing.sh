#!/bin/bash
# $1 = rounds, $2 = optional pin prefix. Prints: variant ns
J27=/opt/jdk/jdk-27/bin/java; J25=/opt/jdk/jdk-25.0.4.1+1/bin/java
for r in $(seq $1); do
  echo "cpp_clang_libstdc++ $($2 ./cpp_clang | awk '/to_string/{print $2}')"
  echo "cpp_gcc_libstdc++ $($2 ./cpp_gcc | awk '/to_string/{print $2}')"
  echo "cpp_clang_libc++ $($2 ./cpp_libcxx | awk '/to_string/{print $2}')"
  echo "java27_long $($2 $J27 -cp jlong Bench | awk '/Integer.toString/{print $2}')"
  echo "java27_double $($2 $J27 -cp jdbl Bench | awk '/Integer.toString/{print $2}')"
  echo "java25_G1 $($2 $J25 -XX:+UseG1GC -cp jlong Bench | awk '/Integer.toString/{print $2}')"
done
