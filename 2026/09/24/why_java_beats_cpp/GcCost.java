// Per timed run: wall time, process CPU time (all threads), GC count and GC time.
import java.lang.management.*;
public class GcCost {
  static final String[] buf = new String[1024];
  static void fromInt(int n) { final String[] b = buf; for (int i = 0; i < n; i++) b[i & 1023] = Integer.toString(i); }
  static long gcCount() { long c = 0; for (var g : ManagementFactory.getGarbageCollectorMXBeans()) c += g.getCollectionCount(); return c; }
  static long gcMillis() { long c = 0; for (var g : ManagementFactory.getGarbageCollectorMXBeans()) c += g.getCollectionTime(); return c; }
  public static void main(String[] a) {
    var os = (com.sun.management.OperatingSystemMXBean) ManagementFactory.getOperatingSystemMXBean();
    final int N = 100_000_000;
    fromInt(1_000_000);
    System.out.println("Java " + System.getProperty("java.version") + ", heap max " + Runtime.getRuntime().maxMemory() / (1 << 20) + " MB, CPUs " + Runtime.getRuntime().availableProcessors());
    for (int r = 0; r < 5; r++) {
      long c0 = gcCount(), m0 = gcMillis(), cpu0 = os.getProcessCpuTime(), t0 = System.nanoTime();
      fromInt(N);
      long t1 = System.nanoTime(), cpu1 = os.getProcessCpuTime();
      System.out.printf("wall %.2f ns/str  cpu(all threads) %.2f ns/str  GCs %d  GC time %d ms (%.1f%% of wall)%n",
          (double) (t1 - t0) / N, (double) (cpu1 - cpu0) / N, gcCount() - c0, gcMillis() - m0, 100.0 * (gcMillis() - m0) * 1e6 / (t1 - t0));
    }
  }
}
