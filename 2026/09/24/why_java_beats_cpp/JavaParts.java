// Decomposes the cost of buf[i & 1023] = Integer.toString(i).
// Run with: --add-exports java.base/jdk.internal.util=ALL-UNNAMED
import jdk.internal.util.DecimalDigits;

public class JavaParts {
  static final String[] buf = new String[1024];
  static final byte[][] cbuf = new byte[1024][16];
  static final Object[] obuf = new Object[1024];

  // same shape as java.lang.String's instance fields
  static final class Str {
    final byte[] value; int hash; final byte coder; boolean hashIsZero;
    Str(byte[] v) { value = v; coder = 0; }
  }

  // 1. the benchmark as published
  static void full(int n) {
    final String[] b = buf;
    for (int i = 0; i < n; i++) b[i & 1023] = Integer.toString(i);
  }
  // 2. conversion only: same JDK digit code, into a preallocated byte ring buffer
  static void convertOnly(int n) {
    for (int i = 0; i < n; i++) {
      DecimalDigits.uncheckedGetCharsLatin1(i, DecimalDigits.stringSize(i), cbuf[i & 1023]);
    }
  }
  // 3. allocation + reference store only: a byte[8] per iteration
  static void allocArray(int n) {
    final Object[] b = obuf;
    for (int i = 0; i < n; i++) b[i & 1023] = new byte[8];
  }
  // 4. allocation + store of a String-shaped pair (object + byte[8]), no digits
  static void allocPair(int n) {
    final Object[] b = obuf;
    for (int i = 0; i < n; i++) b[i & 1023] = new Str(new byte[8]);
  }
  // 5. like 4, but also write 8 bytes into the array (to defeat zeroing elision questions)
  static void allocPairFill(int n) {
    final Object[] b = obuf;
    for (int i = 0; i < n; i++) {
      byte[] v = new byte[8];
      DecimalDigits.uncheckedGetCharsLatin1(10_000_000 + (i & 0xffff), 8, v);
      b[i & 1023] = new Str(v);
    }
  }

  interface Case { void run(int n); }

  static void bench(String name, Case f) {
    final int N = 100_000_000;
    f.run(1_000_000);
    long best = Long.MAX_VALUE;
    for (int r = 0; r < 5; r++) {
      long t0 = System.nanoTime();
      f.run(N);
      long dt = System.nanoTime() - t0;
      if (dt < best) best = dt;
    }
    System.out.printf("%-34s %6.2f ns/iter%n", name, (double) best / N);
  }

  public static void main(String[] args) {
    System.out.println("Java " + System.getProperty("java.version"));
    String only = args.length > 0 ? args[0] : "";
    if (only.isEmpty() || only.equals("1")) bench("1 full Integer.toString + store", JavaParts::full);
    if (only.isEmpty() || only.equals("2")) bench("2 convert only (no alloc)", JavaParts::convertOnly);
    if (only.isEmpty() || only.equals("3")) bench("3 alloc byte[8] + store", JavaParts::allocArray);
    if (only.isEmpty() || only.equals("4")) bench("4 alloc String-shaped pair + store", JavaParts::allocPair);
    if (only.isEmpty() || only.equals("5")) bench("5 alloc pair + fill 8 digits + store", JavaParts::allocPairFill);
  }
}
