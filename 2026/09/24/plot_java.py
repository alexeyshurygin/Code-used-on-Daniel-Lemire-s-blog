# Plots Integer.toString(i) across JDK releases (21, 25, 26, 27) (default G1 collector)
# from results_linux_xeon_java_versions.txt, in the style of plot.py.
import re
import matplotlib.pyplot as plt
from matplotlib.patches import PathPatch
from matplotlib.path import Path

text = open("results_linux_xeon_java_versions.txt").read()

# each "# -XX:+UseG1GC" block: version line, then the Integer.toString(i) line
data = []
for m in re.finditer(r"# -XX:\+UseG1GC\nJava (\S+).*\nInteger\.toString\(i\)\s+([\d.]+) ns", text):
    data.append((f"JDK {m.group(1)}", float(m.group(2))))

SURFACE, INK, MUTED, GRID = "#fcfcfb", "#0b0b0b", "#52514e", "#e4e3df"
JAVA, OLDER = "#4a3aa7", "#9085e9"
plt.rcParams.update({"font.family": ["Avenir Next", "DejaVu Sans"], "font.size": 17})
fig, ax = plt.subplots(figsize=(9, 6.5), dpi=200)
fig.patch.set_facecolor(SURFACE)
ax.set_facecolor(SURFACE)

w = 0.62
ymax = max(v for _, v in data) * 1.15
rx = 0.07
ry = rx * ymax / len(data) * 1.35

def bar(x, v):
    l, r, k = x - w / 2, x + w / 2, 0.45
    verts = [(l, 0), (l, v - ry), (l, v - ry * k), (l + rx * k, v), (l + rx, v),
             (r - rx, v), (r - rx * k, v), (r, v - ry * k), (r, v - ry), (r, 0), (l, 0)]
    codes = [Path.MOVETO, Path.LINETO, Path.CURVE4, Path.CURVE4, Path.CURVE4,
             Path.LINETO, Path.CURVE4, Path.CURVE4, Path.CURVE4, Path.LINETO, Path.CLOSEPOLY]
    return Path(verts, codes)

for x, (label, v) in enumerate(data):
    latest = x == len(data) - 1
    ax.add_patch(PathPatch(bar(x, v), color=JAVA if latest else OLDER, lw=0))
    ax.text(x, v + ymax * 0.015, f"{v:.1f}", ha="center", va="bottom",
            color=INK, fontsize=18, fontweight="bold")

ax.set_xticks(range(len(data)), [d[0] for d in data], color=INK, fontsize=16)
ax.set_xlim(-0.6, len(data) - 0.4)
ax.set_ylim(0, ymax)
ax.tick_params(axis="y", colors=MUTED, labelsize=15)
ax.tick_params(axis="x", length=0, pad=10)
ax.grid(axis="y", color=GRID, lw=1)
ax.set_axisbelow(True)
for s in ax.spines.values():
    s.set_visible(False)
ax.axhline(0, color=MUTED, lw=1)
fig.tight_layout(rect=(0, 0, 1, 0.84))
fig.text(0.015, 0.955, "Java: Integer.toString(i) by JDK release", ha="left", va="top",
         fontsize=24, fontweight="bold", color=INK)
fig.text(0.015, 0.885, "Intel Xeon 2.1 GHz, default G1 GC, ns per string, lower is better",
         ha="left", va="top", fontsize=15, color=MUTED)
fig.savefig("java_versions.png", facecolor=SURFACE)
