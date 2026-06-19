#!/usr/bin/env bash
# run_remise_sabre.sh — run ONE remise_sabre experiment at a chosen latency.
#
# Same role/arg convention as remise: [p0|p1] <log_nitems> <leafsize>
# <auth_fraction> <num_requests>; P0 is the server on 9200, P1 connects via
# P0_HOST. Forwards REMISE_RTT_MS (the RTT) and REMISE_TRIAL into the containers
# so the binary can stamp latency_ms/trial into the Fig 3b / Fig 4b CSVs.
#
# Usage:
#   ./run_remise_sabre.sh <RTT_ms> [bandwidth] [log_nitems] [leafsize] [auth_fraction] [num_requests]
# Defaults: log_nitems=20  leafsize=8  auth_fraction=1.0  num_requests=10

set -euo pipefail
cd "$(dirname "$0")"

BIN=./remise_sabre

RTT="${1:?usage: ./run_remise_sabre.sh <RTT_ms> [bw] [log_nitems] [leafsize] [auth_fraction] [num_requests]}"
BW="${2:-}"
LOG_NITEMS="${3:-20}"
LEAFSIZE="${4:-8}"
AUTH_FRACTION="${5:-1.0}"
NUM_REQUESTS="${6:-10}"
ARGS="$LOG_NITEMS $LEAFSIZE $AUTH_FRACTION $NUM_REQUESTS"

for c in p0 p1; do
  state=$(docker inspect -f '{{.State.Running}}' "$c" 2>/dev/null || echo missing)
  if [[ "$state" != "true" ]]; then
    echo "ERROR: container '$c' is not running (state: $state). Run ./up.sh first." >&2
    exit 1
  fi
done

echo "==> Shaping network: RTT=${RTT}ms ${BW:+bw=$BW}"
./netshape.sh set "$RTT" ${BW:+"$BW"}

echo "==> Launching: $BIN  log_nitems=$LOG_NITEMS leafsize=$LEAFSIZE auth=$AUTH_FRACTION reqs=$NUM_REQUESTS"

# P0 (server) in the background; forward RTT + trial so the CSV is labeled.
docker exec -d -e REMISE_RTT_MS="$RTT" -e REMISE_TRIAL="${REMISE_TRIAL:-0}" \
        p0 sh -c "$BIN p0 $ARGS"

sleep 1

# P1 (client) in the foreground so this script blocks until the run finishes.
docker exec -e REMISE_RTT_MS="$RTT" -e REMISE_TRIAL="${REMISE_TRIAL:-0}" \
        p1 sh -c "P0_HOST=p0 $BIN p1 $ARGS"

echo "==> Experiment finished. Clearing shaping."
./netshape.sh clear
echo "==> Results in ./results/ (online_compute.csv, fig3b_remisebb_sabre.csv, fig4b_remisebb_sabre.csv)."