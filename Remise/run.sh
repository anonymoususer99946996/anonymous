#!/usr/bin/env bash
# run.sh — run ONE experiment at a chosen latency.
#
# It (1) applies the requested RTT/bandwidth, (2) launches P0 in the background
# and P1 in the foreground inside the running containers, (3) waits for the run
# to finish, then (4) clears the shaping.
#
# Containers must already be up (run ./up.sh first).
#
# Usage:
#   ./run.sh <RTT_ms> [bandwidth] [log_nitems] [leafsize] [auth_fraction] [num_requests] [batch_size]
#
# Defaults: log_nitems=20  leafsize=8  auth_fraction=1.0  num_requests=10  batch_size=64
#
# Examples:
#   ./run.sh 0                          # LAN, default experiment params
#   ./run.sh 80 100mbit                 # 80 ms RTT, 100 Mbit/s
#   ./run.sh 80 100mbit 22 64 0.5 50    # custom params, default batch
#   ./run.sh 10 100mbit 16 2 1.0 1 1    # Fig 3a point: 1 request, batch_size 1

set -euo pipefail
cd "$(dirname "$0")"

BIN=./remise

RTT="${1:?usage: ./run.sh <RTT_ms> [bw] [log_nitems] [leafsize] [auth_fraction] [num_requests] [batch_size]}"
BW="${2:-}"
LOG_NITEMS="${3:-20}"
LEAFSIZE="${4:-8}"
AUTH_FRACTION="${5:-1.0}"
NUM_REQUESTS="${6:-10}"
BATCH_SIZE="${7:-64}"

# Both parties MUST receive identical experiment params (only the role differs).
ARGS="$LOG_NITEMS $LEAFSIZE $AUTH_FRACTION $NUM_REQUESTS $BATCH_SIZE"

# Sanity: containers must be running.
for c in p0 p1; do
  state=$(docker inspect -f '{{.State.Running}}' "$c" 2>/dev/null || echo missing)
  if [[ "$state" != "true" ]]; then
    echo "ERROR: container '$c' is not running (state: $state). Run ./up.sh first." >&2
    exit 1
  fi
done

echo "==> Shaping network: RTT=${RTT}ms ${BW:+bw=$BW}"
./netshape.sh set "$RTT" ${BW:+"$BW"}

echo "==> Launching experiment:  log_nitems=$LOG_NITEMS leafsize=$LEAFSIZE auth=$AUTH_FRACTION reqs=$NUM_REQUESTS batch=$BATCH_SIZE"

# P0 starts its server in the background; give it a moment to bind port 9200.
docker exec -d -e REMISE_RTT_MS="$RTT" -e REMISE_TRIAL="${REMISE_TRIAL:-0}" \
        p0 sh -c "$BIN p0 $ARGS"

sleep 1

# P1 runs in the foreground so this script blocks until the experiment finishes.
docker exec -e REMISE_RTT_MS="$RTT" -e REMISE_TRIAL="${REMISE_TRIAL:-0}" \
        p1 sh -c "P0_HOST=p0 $BIN p1 $ARGS"

echo "==> Experiment finished. Clearing shaping."
./netshape.sh clear

echo "==> Results appended to ./results/ (online_compute.csv and, for RemiseBB, results/fig3a_remisebb.csv)."