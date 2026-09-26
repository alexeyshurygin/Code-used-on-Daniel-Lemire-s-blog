public class AllocSize {
  static final String[] buf = new String[1024];
  public static void main(String[] a) {
    var mx = (com.sun.management.ThreadMXBean) java.lang.management.ManagementFactory.getThreadMXBean();
    long tid = Thread.currentThread().threadId();
    for (int i = 0; i < 20_000_000; i++) buf[i & 1023] = Integer.toString(i); // warm up / JIT
    final int N = 10_000_000, base = 10_000_000; // all 8-digit numbers
    long b0 = mx.getThreadAllocatedBytes(tid);
    for (int i = 0; i < N; i++) buf[i & 1023] = Integer.toString(base + i);
    long b1 = mx.getThreadAllocatedBytes(tid);
    System.out.printf("%.1f bytes allocated per 8-digit string%n", (double) (b1 - b0) / N);
  }
}
