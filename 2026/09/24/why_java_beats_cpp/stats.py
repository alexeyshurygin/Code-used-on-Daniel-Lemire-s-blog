import collections,statistics as st,sys
d=collections.defaultdict(list)
for l in open(sys.argv[1]):
    p=l.split()
    if len(p)==2: d[p[0]].append(float(p[1]))
print(f"{'variant':22s} {'n':>2} {'min':>6} {'median':>6} {'max':>6}")
for k,v in d.items(): print(f"{k:22s} {len(v):2d} {min(v):6.2f} {st.median(v):6.2f} {max(v):6.2f}")
