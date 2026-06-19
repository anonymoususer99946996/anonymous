#!/usr/bin/env bash
# sweep_fig3b.sh — RemiseBB (Sabre) audit-time sweep for Figure 3b.
# For each latency (10/30/60 ms) and DB size (2^16..2^26), run 30 single-request
# trials of remise_sabre. Each trial appends one row to
# results/fig3b_remisebb_sabre.csv (variant,latency_ms,log_nitems,trial,audit_ms)
# via run_remise_sabre.sh (which forwards REMISE_RTT_MS and REMISE_TRIAL).
#
# Usage: ./sweep_fig3b.sh            # full sweep
#        ./sweep_fig3b.sh 18 3       # cap log2(DB) at 18, 3 trials (quick test)

set -euo pipefail
cd "$(dirname "$0")"

MAXK="${1:-26}"
TRIALS="${2:-5}"
LEAFSIZE=2          # 32-byte messages
AUTH=1.0            # one authorized request (so the audit runs and is recorded)
REQS=1             # one request per run
BW=100mbit

CSV=results/fig3b_remisebb_sabre.csv
mkdir -p results
echo "variant,latency_ms,log_nitems,trial,audit_ms" > "$CSV"

for L in 10 30 60; do
  for ((k=16; k<=MAXK; k+=2)); do
    for ((tr=1; tr<=TRIALS; tr++)); do
      echo "[fig3b] L=${L}ms  log_nitems=${k}  trial=${tr}/${TRIALS}"
      REMISE_TRIAL="$tr" ./run_remise_sabre.sh "$L" "$BW" "$k" "$LEAFSIZE" "$AUTH" "$REQS"
    done
  done
done

echo "==> Done. Data in $CSV"
