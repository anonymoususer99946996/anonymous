#!/usr/bin/env bash
# sweep_msgsize.sh — message-size experiment.
# Vary message size (leafsize in {2,10,64} = 32/160/1024 B); DB fixed at 2^20;
# latency 30 ms. Both binaries. Outputs preprocess (eval), audit, access
# (finalize+FCW+DB-update) per trial.
#   results/msgsweep_remisebb.csv         (variant,latency_ms,leafsize,log_nitems,trial,preprocess_ms,audit_ms,access_ms)
#   results/msgsweep_remisebb_sabre.csv
#
# Usage: ./sweep_msgsize.sh [both|a|b] [TRIALS]
#   ./sweep_msgsize.sh both       # both variants, 30 trials
#   ./sweep_msgsize.sh a 5        # remise only, 5 trials (quick)
set -euo pipefail
cd "$(dirname "$0")"

WHICH="${1:-both}"
TRIALS="${2:-30}"
LOGN=20             # DB size 2^20 (fixed)
AUTH=1.0           # authorized (so eval/finalize/write all run)
REQS=1             # one request per run
BW=100mbit
L=30               # 30 ms RTT (fixed)
declare -a LEAVES=(2 64 256)

run_variant () {
  local script="$1" csv="$2" variant="$3" extra="$4"
  echo "==> $variant : header -> $csv"
  mkdir -p results
  echo "variant,latency_ms,leafsize,log_nitems,trial,preprocess_ms,audit_ms,access_ms" > "$csv"
  for ls in "${LEAVES[@]}"; do
    for ((tr=1; tr<=TRIALS; tr++)); do
      echo "[$variant] leafsize=$ls (msg=$((ls*16))B)  log_nitems=$LOGN  trial=$tr/$TRIALS"
      REMISE_TRIAL="$tr" "./$script" "$L" "$BW" "$LOGN" "$ls" "$AUTH" "$REQS" $extra
    done
  done
}

case "$WHICH" in
  a)    run_variant run.sh              results/msgsweep_remisebb.csv        RemiseBB       "1" ;;  # batch_size 1
  b)    run_variant run_remise_sabre.sh results/msgsweep_remisebb_sabre.csv RemiseBB-Sabre "" ;;
  both) run_variant run.sh              results/msgsweep_remisebb.csv        RemiseBB       "1"
        run_variant run_remise_sabre.sh results/msgsweep_remisebb_sabre.csv RemiseBB-Sabre "" ;;
  *)    echo "usage: ./sweep_msgsize.sh [both|a|b] [TRIALS]" >&2; exit 1 ;;
esac

echo "==> Done."
