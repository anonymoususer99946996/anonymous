#!/usr/bin/env python3
"""
plot_msgsize.py — message-size experiment: stacked preprocess/audit/access vs
message size, at fixed DB (2^20) and 30 ms RTT.

CSV schema (one row per trial):
    variant,latency_ms,leafsize,log_nitems,trial,preprocess_ms,audit_ms,access_ms

Produces a grouped/stacked bar chart (one bar per leafsize, segments =
preprocess / audit / access) and also prints a mean +/- 95% CI table.

Usage:
    python3 plot_msgsize.py results/msgsweep_remisebb.csv --out msg_remisebb --title "RemiseBB"
    python3 plot_msgsize.py results/msgsweep_remisebb_sabre.csv --out msg_sabre --title "RemiseBB (Sabre)"
"""
import argparse, csv, math, os
from collections import defaultdict
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

PARTS = ["preprocess_ms", "audit_ms", "access_ms"]
PART_LABEL = {"preprocess_ms": "preprocess (eval)",
              "audit_ms": "audit",
              "access_ms": "access (finalize+write)"}
PART_COLOR = {"preprocess_ms": "#1f4ed8", "audit_ms": "#e08e0b", "access_ms": "#17a2b8"}


def load(path):
    # data[leafsize][part] = [values]
    data = defaultdict(lambda: defaultdict(list))
    with open(path, newline="") as f:
        for r in csv.reader(f):
            if not r or r[0].strip().lower() == "variant":
                continue
            try:
                ls = int(float(r[2]))
                vals = {"preprocess_ms": float(r[5]),
                        "audit_ms": float(r[6]),
                        "access_ms": float(r[7])}
            except (IndexError, ValueError):
                continue
            for p in PARTS:
                data[ls][p].append(vals[p])
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


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("csv")
    ap.add_argument("--out", default="msgsize")
    ap.add_argument("--title", default="RemiseBB")
    args = ap.parse_args()
    if not os.path.exists(args.csv):
        raise SystemExit(f"CSV not found: {args.csv}")

    data = load(args.csv)
    leaves = sorted(data.keys())
    if not leaves:
        raise SystemExit("no data rows found")

    # means + CI per (leafsize, part)
    means = {p: [] for p in PARTS}
    cis = {p: [] for p in PARTS}
    for ls in leaves:
        for p in PARTS:
            m, h = mean_ci(data[ls][p])
            means[p].append(m)
            cis[p].append(h)

    # ---- print table ----
    print(f"\n{args.title}: mean +/- 95% CI (ms), DB=2^20, 30 ms RTT")
    print(f"{'msg(B)':>8} {'leaf':>5} | "
          + " | ".join(f"{PART_LABEL[p]:>24}" for p in PARTS))
    for i, ls in enumerate(leaves):
        row = f"{ls*16:>8} {ls:>5} | "
        row += " | ".join(f"{means[p][i]:>10.3f} +/- {cis[p][i]:>7.3f}" for p in PARTS)
        print(row)
    print()

    # ---- stacked bar chart ----
    x = list(range(len(leaves)))
    fig, ax = plt.subplots(figsize=(7, 4.5))
    bottom = [0.0] * len(leaves)
    for p in PARTS:
        ax.bar(x, means[p], bottom=bottom, width=0.6,
               color=PART_COLOR[p], label=PART_LABEL[p],
               yerr=cis[p], capsize=3, error_kw={"elinewidth": 1})
        bottom = [b + m for b, m in zip(bottom, means[p])]
    ax.set_xticks(x)
    ax.set_xticklabels([f"{ls*16} B\n(leaf {ls})" for ls in leaves])
    ax.set_xlabel("Message size")
    ax.set_ylabel("time (ms)")
    ax.set_title(f"{args.title} — DB $2^{{20}}$, 30 ms RTT")
    ax.legend()
    ax.grid(True, axis="y", linestyle=":", linewidth=0.5)
    fig.tight_layout()
    fig.savefig(f"{args.out}.pdf")
    fig.savefig(f"{args.out}.png", dpi=150)
    print(f"wrote {args.out}.pdf (+ .png)")


if __name__ == "__main__":
    main()
