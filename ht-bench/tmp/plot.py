#!/usr/bin/env python3plot
"""Plot hyperfine results comparing extlib.h (simd) vs extlib.h.

Produces two figures saved to tmp/:
  bench_absolute.png  — grouped horizontal bar chart of mean wall time
  bench_speedup.png   — speedup ratio (extlib (simd) time / extlib time)
"""

import json
import glob
import os
import numpy as np
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import matplotlib.ticker as ticker

# ── Load results ──────────────────────────────────────────────────────────────

# Canonical benchmark order, grouped by key/value kind.
ORDER = [
    # integer keys, int value
    "insert-int",
    "lookup-hit-int",
    "lookup-miss-int",
    "delete-int",
    "delete-heavy-int",
    "mixed-int",
    "iterate-int",
    "word-count-int",
    # string keys, int value
    "insert-str",
    "lookup-hit-str",
    "lookup-miss-str",
    "delete-heavy-str",
    # 64-byte keys, int value
    "insert-bigkey",
    "lookup-hit-bigkey",
    "delete-heavy-bigkey",
    # integer keys, 64-byte value
    "insert-large",
    "lookup-hit-large",
    "delete-heavy-large",
]

# Group boundary indices and labels shown as separator lines.
GROUPS = [
    (0,  "int key / int value"),
    (8,  "str key / int value"),
    (12, "64-byte key / int value"),
    (15, "int key / 64-byte value"),
]

raw = {}
for path in glob.glob(os.path.join(os.path.dirname(__file__), "*.json")):
    bm = os.path.splitext(os.path.basename(path))[0]
    with open(path) as f:
        data = json.load(f)
    by_cmd = {r["command"]: r for r in data["results"]}
    # Convert seconds → milliseconds
    raw[bm] = {
        "ht":     {"mean": by_cmd["extlib_simd"]["mean"] * 1e3,
                   "sd":   by_cmd["extlib_simd"]["stddev"] * 1e3,
                   "times": [t * 1e3 for t in by_cmd["extlib_simd"]["times"]]},
        "extlib": {"mean": by_cmd["extlib"]["mean"] * 1e3,
                   "sd":   by_cmd["extlib"]["stddev"] * 1e3,
                   "times": [t * 1e3 for t in by_cmd["extlib"]["times"]]},
    }

benchmarks = [b for b in ORDER if b in raw]

ht_mean  = np.array([raw[b]["ht"]["mean"]  for b in benchmarks])
ht_sd    = np.array([raw[b]["ht"]["sd"]    for b in benchmarks])
ext_mean = np.array([raw[b]["extlib"]["mean"] for b in benchmarks])
ext_sd   = np.array([raw[b]["extlib"]["sd"]   for b in benchmarks])
speedup  = ht_mean / ext_mean   # >1 → extlib faster, <1 → ht faster

HT_COL     = "#4c72b0"
EXT_COL    = "#dd8452"
FASTER_COL = "#2e7d32"
SLOWER_COL = "#c62828"

OUT_DIR = os.path.dirname(__file__)

# ── Figure 1: absolute times ──────────────────────────────────────────────────

fig, ax = plt.subplots(figsize=(13, 10))

y  = np.arange(len(benchmarks))
bh = 0.35

bars_ht  = ax.barh(y + bh/2, ht_mean,  bh, xerr=ht_sd,  label="extlib.h (simd)",
                   color=HT_COL,  capsize=3, error_kw={"elinewidth": 1, "capthick": 1})
bars_ext = ax.barh(y - bh/2, ext_mean, bh, xerr=ext_sd, label="extlib.h",
                   color=EXT_COL, capsize=3, error_kw={"elinewidth": 1, "capthick": 1})

# Annotate each pair with the speedup ratio at the right edge.
x_max = max(ht_mean.max(), ext_mean.max())
for i, sp in enumerate(speedup):
    color = FASTER_COL if sp > 1 else SLOWER_COL
    label = f"{sp:.2f}×"
    ax.text(x_max * 1.01, i, label, va="center", ha="left",
            fontsize=8.5, color=color, fontweight="bold")

# Group separators and centred labels.
boundaries = [g[0] for g in GROUPS] + [len(benchmarks)]
for idx, (start, label) in enumerate(GROUPS):
    if start > 0:
        ax.axhline(start - 0.5, color="grey", linewidth=0.8, linestyle=":")
    mid = (start + boundaries[idx + 1] - 1) / 2
    ax.text(-1.5, mid, label, ha="right", va="center",
            fontsize=8, color="#555555", style="italic")

ax.set_yticks(y)
ax.set_yticklabels(benchmarks, fontsize=10)
ax.set_xlabel("mean wall time  (ms, lower is better)", fontsize=10)
ax.xaxis.set_major_formatter(ticker.FormatStrFormatter("%.0f ms"))
ax.legend(fontsize=10, loc="lower right")
ax.grid(axis="x", linestyle="--", linewidth=0.5, alpha=0.5)
ax.set_xlim(left=0, right=x_max * 1.14)
ax.set_title("extlib.h (simd) vs extlib.h — absolute wall time  (N=500 000, -O3)\n"
             "ratio label = ht÷extlib  (green: extlib faster, red: extlib (simd) faster)",
             fontsize=11)

fig.tight_layout()
out1 = os.path.join(OUT_DIR, "bench_absolute.png")
fig.savefig(out1, dpi=150, bbox_inches="tight")
print(f"saved {out1}")

# ── Figure 2: speedup ratio ───────────────────────────────────────────────────

fig2, ax2 = plt.subplots(figsize=(10, 8))

bar_colors = [FASTER_COL if s > 1 else SLOWER_COL for s in speedup]
ax2.barh(y, speedup, 0.55, color=bar_colors, edgecolor="white", linewidth=0.4)
ax2.axvline(1.0, color="black", linewidth=1.2, linestyle="--", label="parity (1.0×)")

# Label each bar.
for i, sp in enumerate(speedup):
    ha = "left" if sp >= 1 else "right"
    offset = 0.02 if sp >= 1 else -0.02
    ax2.text(sp + offset, i, f"{sp:.2f}×",
             va="center", ha=ha, fontsize=9,
             color=bar_colors[i], fontweight="bold")

for start, _label in GROUPS:
    if start > 0:
        ax2.axhline(start - 0.5, color="grey", linewidth=0.8, linestyle=":")
for idx, (start, label) in enumerate(GROUPS):
    mid = (start + boundaries[idx + 1] - 1) / 2
    ax2.text(-0.05, mid, label, ha="right", va="center",
             fontsize=8, color="#555555", style="italic")

ax2.set_yticks(y)
ax2.set_yticklabels(benchmarks, fontsize=10)
ax2.set_xlabel("ht time ÷ extlib time  (>1 = extlib wins, <1 = extlib (simd) wins)", fontsize=10)
ax2.xaxis.set_major_formatter(ticker.FormatStrFormatter("%.2f×"))
ax2.legend(fontsize=9)
ax2.grid(axis="x", linestyle="--", linewidth=0.5, alpha=0.5)
ax2.set_xlim(left=0)
ax2.set_title("Relative speedup: extlib.h (simd) vs extlib.h  (N=500 000, -O3)\n"
              "green: extlib.h faster  |  red: extlib.h (simd) faster",
              fontsize=11)

fig2.tight_layout()
out2 = os.path.join(OUT_DIR, "bench_speedup.png")
fig2.savefig(out2, dpi=150, bbox_inches="tight")
print(f"saved {out2}")
