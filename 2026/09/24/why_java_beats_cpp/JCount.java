// For instruction counting under valgrind: warm up, then run K iterations (K from args).
public class JCount {
  static final String[] buf = new String[1024];
  static void fromInt(int from, int n) {
    final String[] b = buf;
    for (int i = from; i < from + n; i++) b[i & 1023] = Integer.toString(i);
  }
  public static void main(String[] a) {
    for (int r = 0; r < 40; r++) fromInt(0, 500_000);   // warm up: C2-compile fromInt
    fromInt(10_000_000, Integer.parseInt(a[0]));        // 8-digit numbers, like 90% of the benchmark
    int t = 0; for (String s : buf) t += s.length();
    System.out.println(t);
  }
}
