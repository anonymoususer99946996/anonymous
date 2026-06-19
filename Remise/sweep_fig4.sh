#!/usr/bin/env bash
# sweep_fig4.sh — throughput/goodput sweep for Figures 4a (RemiseBB) and 4b
# (RemiseBB Sabre). For each unauthorized fraction (30/60/90%) and DB size
# (2^16..2^26), run TRIALS runs of REQS requests each. A single run emits all
# four curve values (throughput, goodput, online_throughput, online_goodput)
# to results/fig4a_remisebb.csv / results/fig4b_remisebb_sabre.csv.
#
# Usage: ./sweep_fig4.sh [a|b|both] [MAXK] [TRIALS] [REQS]
#   ./sweep_fig4.sh both          # full: 4a and 4b, 2^26, 5 trials, 200 reqs
#   ./sweep_fig4.sh a 18 2 50     # quick 4a test
set -euo pipefail
cd "$(dirname "$0")"

WHICH="${1:-both}"
MAXK="${2:-26}"
TRIALS="${3:-5}"
REQS="${4:-20}"
LEAFSIZE=2          # 32-byte messages
BW=100mbit
L=30                # latency fixed at 30 ms (paper's Fig 4 setting); change if needed

# unauthorized fractions -> authorized fractions
#   30% unauth -> auth 0.7 ; 60% -> 0.4 ; 90% -> 0.1
declare -a AUTHS=(0.7 0.4 0.1)

run_variant () {
  local script="$1" csv="$2" variant="$3"
  echo "==> $variant : header -> $csv"
  mkdir -p results
  echo "variant,unauth_frac,log_nitems,trial,throughput,goodput,online_throughput,online_goodput" > "$csv"
  for af in "${AUTHS[@]}"; do
    for ((k=16; k<=MAXK; k+=2)); do
      for ((tr=1; tr<=TRIALS; tr++)); do
        echo "[$variant] unauth=$(awk "BEGIN{printf \"%.0f\", (1-$af)*100}")%  log_nitems=$k  trial=$tr/$TRIALS"
        REMISE_TRIAL="$tr" "./$script" "$L" "$BW" "$k" "$LEAFSIZE" "$af" "$REQS"
      done
    done
  done
}

case "$WHICH" in
  a)    run_variant run.sh              results/fig4a_remisebb.csv        RemiseBB ;;
  b)    run_variant run_remise_sabre.sh results/fig4b_remisebb_sabre.csv RemiseBB-Sabre ;;
  both) run_variant run.sh              results/fig4a_remisebb.csv        RemiseBB
        run_variant run_remise_sabre.sh results/fig4b_remisebb_sabre.csv RemiseBB-Sabre ;;
  *)    echo "usage: ./sweep_fig4.sh [a|b|both] [MAXK] [TRIALS] [REQS]" >&2; exit 1 ;;
esac

echo "==> Done."
