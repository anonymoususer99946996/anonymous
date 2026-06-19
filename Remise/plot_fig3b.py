#!/usr/bin/env python3
"""
plot_fig3b.py — Figure 3b: RemiseBB (Sabre) audit time vs database size.

Reads results/fig3b_remisebb_sabre.csv with columns:
    variant,latency_ms,log_nitems,trial,audit_ms
and produces three plots (10, 30, 60 ms), each showing the audit-time curve vs
DB size, averaged over trials with 95% CI error bars. (Sabre has no preproc; the
audit is the LowMC encrypt2_p0p1 MPC, expected to be ~flat in DB size.)

Usage:
    python3 plot_fig3b.py [csv_path] [--out-prefix fig3b] [--combined]
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
    rows = defaultdict(list)  # rows[(latency_ms, log_nitems)] = [audit_ms, ...]
    with open(csv_path, newline="") as f:
        for r in csv.reader(f):
            if not r or r[0].strip().lower() == "variant":
                continue
            try:
                latency = int(float(r[1]))
                k = int(float(r[2]))
                audit = float(r[4])
            except (IndexError, ValueError):
                continue
            rows[(latency, k)].append(audit)
    return rows


def mean_ci(vals):
    n = len(vals)
    if n == 0:
        return math.nan, 0.0
    m = sum(vals) / n
    if n < 2:
        return m, 0.0
    sd = math.sqrt(sum((v - m) ** 2 for v in vals) / (n - 1))
    return m, 1.96 * sd / math.sqrt(n)


def series_for_latency(rows, latency):
    ks = sorted({k for (lat, k) in rows if lat == latency})
    audit, ci = [], []
    for k in ks:
        m, h = mean_ci(rows[(latency, k)])
        audit.append(m)
        ci.append(h)
    return ks, audit, ci


def plot_one(ax, rows, latency):
    ks, audit, ci = series_for_latency(rows, latency)
    if not ks:
        ax.set_title(f"{latency} ms — no data")
        return
    ax.errorbar(ks, audit, yerr=ci, marker="o", capsize=3,
                label="audit (LowMC MPC)")
    ax.set_xlabel(r"$\log_2$(database size)")
    ax.set_ylabel("audit time (ms)")
    ax.set_title(f"RemiseBB (Sabre) — {latency} ms RTT")
    ax.set_xticks(ks)
    ax.grid(True, which="both", linestyle=":", linewidth=0.5)
    ax.legend()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("csv", nargs="?", default="results/fig3b_remisebb_sabre.csv")
    ap.add_argument("--out-prefix", default="fig3b")
    ap.add_argument("--combined", action="store_true")
    ap.add_argument("--logy", action="store_true",
                    help="use a log y-axis (audit is near-flat; linear is usually clearer)")
    args = ap.parse_args()

    if not os.path.exists(args.csv):
        raise SystemExit(f"CSV not found: {args.csv}")

    rows = load(args.csv)
    latencies = [10, 30, 60]

    for lat in latencies:
        fig, ax = plt.subplots(figsize=(5, 4))
        plot_one(ax, rows, lat)
        if args.logy:
            ax.set_yscale("log")
        fig.tight_layout()
        out = f"{args.out_prefix}_{lat}ms.pdf"
        fig.savefig(out)
        fig.savefig(out.replace(".pdf", ".png"), dpi=150)
        plt.close(fig)
        print(f"wrote {out} (+ .png)")

    if args.combined:
        fig, axes = plt.subplots(1, 3, figsize=(14, 4), sharey=True)
        for ax, lat in zip(axes, latencies):
            plot_one(ax, rows, lat)
            if args.logy:
                ax.set_yscale("log")
        fig.tight_layout()
        out = f"{args.out_prefix}_combined.pdf"
        fig.savefig(out)
        fig.savefig(out.replace(".pdf", ".png"), dpi=150)
        plt.close(fig)
        print(f"wrote {out} (+ .png)")


if __name__ == "__main__":
    main()
