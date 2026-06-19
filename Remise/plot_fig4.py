#!/usr/bin/env python3
"""
plot_fig4.py — Figures 4a / 4b: throughput & goodput vs database size, in a
3-panel row (30% / 60% / 90% unauthorized traffic), four curves per panel:

    throughput          (on-the-fly DPFs)   -- solid
    goodput             (on-the-fly DPFs)   -- dashed
    online_throughput   (with preproc DPFs) -- solid
    online_goodput      (with preproc DPFs) -- dashed

CSV schema (one row per trial):
    variant,unauth_frac,log_nitems,trial,throughput,goodput,online_throughput,online_goodput

Usage:
    python3 plot_fig4.py results/fig4a_remisebb.csv --out fig4a --title "RemiseBB"
    python3 plot_fig4.py results/fig4b_remisebb_sabre.csv --out fig4b --title "RemiseBB (Sabre)"
"""
import argparse
import csv
import math
import os
from collections import defaultdict

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import matplotlib.ticker as mticker

COLS = ["throughput", "goodput", "online_throughput", "online_goodput"]

# label, color, linestyle for each curve (matches the paper's grouping:
# preproc = blue-ish, on-the-fly = teal; throughput solid, goodput dashed)
STYLE = {
    "online_throughput": ("RemiseBB (w/ preproc DPFs) Thrput",  "#1f4ed8", "-"),
    "online_goodput":    ("RemiseBB (w/ preproc DPFs) Goodput", "#1f4ed8", "--"),
    "throughput":        ("RemiseBB (w/ on-the-fly DPFs) Thrput",  "#17a2b8", "-"),
    "goodput":           ("RemiseBB (w/ on-the-fly DPFs) Goodput", "#17a2b8", "--"),
}


def load(csv_path):
    # data[unauth][col][k] = [values...]
    data = defaultdict(lambda: defaultdict(lambda: defaultdict(list)))
    with open(csv_path, newline="") as f:
        for r in csv.reader(f):
            if not r or r[0].strip().lower() == "variant":
                continue
            try:
                unauth = round(float(r[1]), 3)
                k = int(float(r[2]))
                vals = {COLS[i]: float(r[4 + i]) for i in range(4)}
            except (IndexError, ValueError):
                continue
            for c in COLS:
                data[unauth][c][k].append(vals[c])
    return data


def mean_ci(vals):
    n = len(vals)
    if n == 0:
        return math.nan, 0.0
    m = sum(vals) / n
    if n < 2:
        return m, 0.0
    sd = math.sqrt(sum((v - m) ** 2 for v in vals) / (n - 1))
    return m, 1.96 * sd / math.sqrt(n)


def panel(ax, data, unauth, title, label_curves):
    cd = data.get(unauth, {})
    for c in COLS:
        ks = sorted(cd.get(c, {}).keys())
        if not ks:
            continue
        ys, es = [], []
        for k in ks:
            m, h = mean_ci(cd[c][k])
            ys.append(m)
            es.append(h)
        lab, color, ls = STYLE[c]
        ax.errorbar(ks, ys, yerr=es, color=color, linestyle=ls, marker="o",
                    markersize=3, capsize=2, linewidth=1.3,
                    label=(lab if label_curves else None))
    ax.set_yscale("log", base=2)
    ax.yaxis.set_major_formatter(mticker.FuncFormatter(lambda y, _: f"$2^{{{int(round(math.log2(y)))}}}$" if y > 0 else "0"))
    ax.set_xlabel("Database size")
    ax.set_title(title)
    ax.grid(True, which="both", linestyle=":", linewidth=0.4)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("csv")
    ap.add_argument("--out", default="fig4")
    ap.add_argument("--title", default="RemiseBB",
                    help="variant name used in legend labels and suptitle")
    args = ap.parse_args()
    if not os.path.exists(args.csv):
        raise SystemExit(f"CSV not found: {args.csv}")

    # Re-label STYLE legends to the requested variant name
    for c in COLS:
        lab, color, ls = STYLE[c]
        STYLE[c] = (lab.replace("RemiseBB", args.title), color, ls)

    data = load(args.csv)
    # panels for 30/60/90% unauthorized -> auth 0.7/0.4/0.1 -> unauth 0.3/0.6/0.9
    panels = [(0.3, "30% unauthorized traffic"),
              (0.6, "60% unauthorized traffic"),
              (0.9, "90% unauthorized traffic")]

    fig, axes = plt.subplots(1, 3, figsize=(15, 4.2), sharey=True)
    for i, (ax, (uf, title)) in enumerate(zip(axes, panels)):
        panel(ax, data, uf, title, label_curves=(i == 0))
    axes[0].set_ylabel("Ops per second")

    # Single shared legend on top
    handles, labels = axes[0].get_legend_handles_labels()
    fig.legend(handles, labels, loc="upper center", ncol=2, fontsize=8,
               frameon=False, bbox_to_anchor=(0.5, 1.08))
    fig.tight_layout(rect=[0, 0, 1, 0.93])

    fig.savefig(f"{args.out}.pdf", bbox_inches="tight")
    fig.savefig(f"{args.out}.png", dpi=150, bbox_inches="tight")
    print(f"wrote {args.out}.pdf (+ .png)")


if __name__ == "__main__":
    main()
