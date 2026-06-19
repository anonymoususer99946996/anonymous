#!/usr/bin/env bash
# sweep_fig3a.sh — RemiseBB validity-check sweep for Figure 3a.
# For each latency (10/30/60 ms) and DB size (2^16..2^26), run 30 single-request
# trials. Each trial appends one row to results/fig3a_remisebb.csv via the binary
# (which reads REMISE_RTT_MS and REMISE_TRIAL from the environment).
#
# Usage: ./sweep_fig3a.sh            # full sweep
#        ./sweep_fig3a.sh 26 5       # cap log2(DB) at 26, 5 trials (quick test)

set -euo pipefail
cd "$(dirname "$0")"

MAXK="${1:-26}"
TRIALS="${2:-5}"
LEAFSIZE=2          # 32-byte messages
AUTH=1.0            # authorized traffic
REQS=1             # one request per run
BW=100mbit

CSV=results/fig3a_remisebb.csv
mkdir -p results
echo "variant,latency_ms,log_nitems,trial,preproc_ms,total_ms" > "$CSV"

for L in 10 30 60; do
  for ((k=16; k<=MAXK; k+=2)); do
    for ((tr=1; tr<=TRIALS; tr++)); do
      echo "[fig3a] L=${L}ms  log_nitems=${k}  trial=${tr}/${TRIALS}"
      # run.sh exports REMISE_RTT_MS and REMISE_TRIAL into the containers.
      REMISE_TRIAL="$tr" ./run.sh "$L" "$BW" "$k" "$LEAFSIZE" "$AUTH" "$REQS"
    done
  done
done

echo "==> Done. Data in $CSV"
