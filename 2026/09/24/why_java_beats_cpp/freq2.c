// Dependent imul chain: 3-cycle latency per imul on Intel P-cores, so GHz = 3*imuls/ns.
#include <stdio.h>
#include <time.h>
static double now(){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec*1e9+t.tv_nsec;}
int main(){ double best=1e300; long n=300000000;
 for(int r=0;r<5;r++){ long x=3; double t0=now();
  for(long i=0;i<n;i+=10) __asm__ volatile("imul %0,%0\n\timul %0,%0\n\timul %0,%0\n\timul %0,%0\n\timul %0,%0\n\timul %0,%0\n\timul %0,%0\n\timul %0,%0\n\timul %0,%0\n\timul %0,%0":"+r"(x));
  double dt=now()-t0; if(dt<best)best=dt; }
 printf("%.2f GHz (3-cycle imul chain)\n", 3.0*n/best); }
