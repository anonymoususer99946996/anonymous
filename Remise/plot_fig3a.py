#!/usr/bin/env python3
"""
plot_fig3a.py — Figure 3a: RemiseBB validity-check time vs database size.

Reads results/fig3a_remisebb.csv with columns:
    variant,latency_ms,log_nitems,trial,online_ms,total_ms
and produces three plots (one per latency: 10, 30, 60 ms), each showing the
'online' (audit only) and 'total' (eval + audit) curves vs DB size, averaged
over trials with 95% confidence-interval error bars.

Usage:
    python3 plot_fig3a.py [csv_path] [--out-prefix fig3a] [--combined]
"""
import argparse
import csv
import math
import os
from collections import defaultdict

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt


def load(csv_path):
    # rows[(latency_ms, log_nitems)] = {"online": [...], "total": [...]}
    rows = defaultdict(lambda: {"online": [], "total": []})
    with open(csv_path, newline="") as f:
        reader = csv.reader(f)
        for r in reader:
            if not r or r[0].strip().lower() == "variant":
                continue  # skip header / blank
            try:
                latency = int(float(r[1]))
                k = int(float(r[2]))
                online = float(r[4])
                total = float(r[5])
            except (IndexError, ValueError):
                continue
            rows[(latency, k)]["online"].append(online)
            rows[(latency, k)]["total"].append(total)
    return rows


def mean_ci(vals):
    """Return (mean, half-width of 95% CI). Falls back to 0 CI for n<2."""
    n = len(vals)
    if n == 0:
        return math.nan, 0.0
    m = sum(vals) / n
    if n < 2:
        return m, 0.0
    var = sum((v - m) ** 2 for v in vals) / (n - 1)
    sd = math.sqrt(var)
    se = sd / math.sqrt(n)
    return m, 1.96 * se  # normal approx; fine for n=30


def series_for_latency(rows, latency):
    ks = sorted({k for (lat, k) in rows if lat == latency})
    out = {"k": ks, "online": [], "online_ci": [], "total": [], "total_ci": []}
    for k in ks:
        om, oci = mean_ci(rows[(latency, k)]["online"])
        tm, tci = mean_ci(rows[(latency, k)]["total"])
        out["online"].append(om)
        out["online_ci"].append(oci)
        out["total"].append(tm)
        out["total_ci"].append(tci)
    return out


def plot_one(ax, rows, latency):
    s = series_for_latency(rows, latency)
    if not s["k"]:
        ax.set_title(f"{latency} ms — no data")
        return
    x = s["k"]
    ax.errorbar(x, s["online"], yerr=s["online_ci"], marker="o",
                capsize=3, label="online (audit)")
    ax.errorbar(x, s["total"], yerr=s["total_ci"], marker="s",
                capsize=3, label="total (eval + audit)")
    ax.set_yscale("log")
    ax.set_xlabel(r"$\log_2$(database size)")
    ax.set_ylabel("validity-check time (ms)")
    ax.set_title(f"RemiseBB — authorized traffic, {latency} ms RTT")
    ax.set_xticks(x)
    ax.grid(True, which="both", linestyle=":", linewidth=0.5)
    ax.legend()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("csv", nargs="?", default="results/fig3a_remisebb.csv")
    ap.add_argument("--out-prefix", default="fig3a")
    ap.add_argument("--combined", action="store_true",
                    help="also emit a single 1x3 panel figure")
    args = ap.parse_args()

    if not os.path.exists(args.csv):
        raise SystemExit(f"CSV not found: {args.csv}")

    rows = load(args.csv)
    latencies = [10, 30, 60]

    # One figure per latency
    for lat in latencies:
        fig, ax = plt.subplots(figsize=(5, 4))
        plot_one(ax, rows, lat)
        fig.tight_layout()
        out = f"{args.out_prefix}_{lat}ms.pdf"
        fig.savefig(out)
        fig.savefig(out.replace(".pdf", ".png"), dpi=150)
        plt.close(fig)
        print(f"wrote {out} (+ .png)")

    # Optional combined panel
    if args.combined:
        fig, axes = plt.subplots(1, 3, figsize=(14, 4), sharey=True)
        for ax, lat in zip(axes, latencies):
            plot_one(ax, rows, lat)
        fig.tight_layout()
        out = f"{args.out_prefix}_combined.pdf"
        fig.savefig(out)
        fig.savefig(out.replace(".pdf", ".png"), dpi=150)
        plt.close(fig)
        print(f"wrote {out} (+ .png)")


if __name__ == "__main__":
    main()