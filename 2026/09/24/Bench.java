public class Bench {
  static final String[] buf = new String[1024];

  static void fromInt(int n) {
    final String[] b = buf;
    for (int i = 0; i < n; i++) b[i & 1023] = Integer.toString(i);
  }

  static void concat(int n) {
    final String[] b = buf;
    for (int i = 0; i < n; i++) b[i & 1023] = "" + i;
  }

  interface Case { void run(int n); }

  static void bench(String name, Case f) {
    final int N = 100_000_000;
    f.run(1_000_000); // warm up
    double best = Double.MAX_VALUE;
    for (int r = 0; r < 5; r++) {
      long t0 = System.nanoTime();
      f.run(N);
      double dt = System.nanoTime() - t0;
      if (dt < best) best = dt;
    }
    int total = 0;
    for (String s : buf) total += s.length();
    double ns = best / N;
    System.out.printf("%-24s %7.2f ns/string  %8.1f M/s   (check %d)%n",
                      name, ns, 1e3 / ns, total);
  }

  public static void main(String[] args) {
    System.out.println("Java " + System.getProperty("java.version") + " ("
        + System.getProperty("java.vm.name") + ", "
        + java.lang.management.ManagementFactory.getGarbageCollectorMXBeans().get(0).getName() + ")");
    bench("Integer.toString(i)", Bench::fromInt);
    bench("\"\" + i", Bench::concat);
  }
}
