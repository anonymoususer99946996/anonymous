#!/usr/bin/env bash
#
# run_experiment.sh — drive the Remise two-party online benchmark and
# aggregate per-run statistics.
#
# Usage:
#   ./run_experiment.sh <log_nitems> <leafsize> <authorized_fraction> <num_requests> <num_runs>
#
# Optional environment variables:
#   REMISE_BIN    path to the remise binary            (default: ./remise)
#   PORT          TCP port P0 listens on               (default: 9200)
#   RUN_TIMEOUT   per-party wall-clock cap, seconds    (default: 0 = no limit)
#   OUTDIR        output directory                     (default: results)
#
# Output:
#   results/raw_<ts>.log    concatenated P0/P1 stdout for every run
#   results/stats_<ts>.csv  one row per run (schema unchanged from the original)

set -euo pipefail

# ----------------------------------------------------------------------------
# Argument handling
# ----------------------------------------------------------------------------

if [ "$#" -ne 5 ]; then
    cat >&2 <<EOF
Usage:
  $0 <log_nitems> <leafsize> <authorized_fraction> <num_requests> <num_runs>
EOF
    exit 1
fi

LOG_NITEMS=$1
LEAFSIZE=$2
AUTH_FRAC=$3
NUM_REQUESTS=$4
NUM_RUNS=$5

REMISE_BIN=${REMISE_BIN:-./remise}
PORT=${PORT:-9200}
RUN_TIMEOUT=${RUN_TIMEOUT:-0}
OUTDIR=${OUTDIR:-results}

if [ ! -x "$REMISE_BIN" ]; then
    echo "error: remise binary not found or not executable: $REMISE_BIN" >&2
    echo "       build it first, or set REMISE_BIN=/path/to/remise" >&2
    exit 1
fi

if ! [[ "$NUM_RUNS" =~ ^[0-9]+$ ]] || [ "$NUM_RUNS" -lt 1 ]; then
    echo "error: num_runs must be a positive integer" >&2
    exit 1
fi

mkdir -p "$OUTDIR"

TIMESTAMP=$(date +"%Y%m%d_%H%M%S")
RAWFILE="$OUTDIR/raw_${TIMESTAMP}.log"
CSVFILE="$OUTDIR/stats_${TIMESTAMP}.csv"

echo "Binary        : $REMISE_BIN"
echo "Port          : $PORT"
echo "Raw logs      : $RAWFILE"
echo "CSV stats     : $CSVFILE"
[ "$RUN_TIMEOUT" -gt 0 ] 2>/dev/null && echo "Per-run timeout: ${RUN_TIMEOUT}s"

# ----------------------------------------------------------------------------
# Cleanup: make sure a backgrounded P0 never outlives the script
# ----------------------------------------------------------------------------

P0_PID=""
cleanup() {
    if [ -n "$P0_PID" ] && kill -0 "$P0_PID" 2>/dev/null; then
        kill "$P0_PID" 2>/dev/null || true
        wait "$P0_PID" 2>/dev/null || true
    fi
}
trap cleanup EXIT INT TERM

# ----------------------------------------------------------------------------
# Helpers
# ----------------------------------------------------------------------------

# Optionally wrap a command in `timeout` when RUN_TIMEOUT > 0 and timeout exists.
maybe_timeout() {
    if [ "$RUN_TIMEOUT" -gt 0 ] 2>/dev/null && command -v timeout >/dev/null 2>&1; then
        timeout --signal=TERM "$RUN_TIMEOUT" "$@"
    else
        "$@"
    fi
}

# Wait until P0 is actually listening, instead of a blind `sleep 1`.
# Falls back to a fixed sleep when `ss` is unavailable.
wait_for_listen() {
    local port="$1" tries=200   # up to ~20s
    if command -v ss >/dev/null 2>&1; then
        for ((i = 0; i < tries; i++)); do
            if ss -ltn 2>/dev/null | grep -q ":${port}[[:space:]]"; then
                return 0
            fi
            # bail early if P0 already died
            if [ -n "$P0_PID" ] && ! kill -0 "$P0_PID" 2>/dev/null; then
                return 1
            fi
            sleep 0.1
        done
        return 1
    else
        sleep 1
    fi
}

# Extract the numeric value for an EXACT summary label.
#
# Matches the trimmed text *before the first colon* against the label, so
# "Throughput" no longer collides with "--- Throughput ---" or
# "Online throughput", and "Average write latency" no longer collides with
# "Average write latency (for PACL)". Always exits 0; prints "" when missing.
extract_number() {
    local label="$1" file="$2"
    awk -F':' -v lbl="$label" '
        {
            key = $1
            sub(/^[ \t]+/, "", key); sub(/[ \t]+$/, "", key)
            if (key == lbl && NF >= 2) {
                if (match($2, /-?[0-9]+(\.[0-9]+)?([eE][+-]?[0-9]+)?/))
                    val = substr($2, RSTART, RLENGTH)
            }
        }
        END { print val }
    ' "$file"
}

# ----------------------------------------------------------------------------
# CSV header (schema identical to the original)
# ----------------------------------------------------------------------------

echo "run,\
authorized_fraction,\
total_requests,\
authorized_requests,\
unauthorized_requests,\
avg_eval_ms,\
avg_audit_ms,\
avg_write_ms,\
avg_pacl_write_ms,\
avg_fcw_exchange_ms,\
throughput,\
goodput,\
online_throughput,\
online_goodput,\
eval_bandwidth,\
audit_bandwidth,\
write_bandwidth,\
total_bandwidth" \
> "$CSVFILE"

# ----------------------------------------------------------------------------
# Experiment loop
# ----------------------------------------------------------------------------

for ((run = 1; run <= NUM_RUNS; run++)); do

    echo "======================================" | tee -a "$RAWFILE"
    echo "RUN $run / $NUM_RUNS"                    | tee -a "$RAWFILE"
    echo "======================================" | tee -a "$RAWFILE"

    rm -f p0.log p1.log

    # --- launch P0 (server) in the background --------------------------------
    maybe_timeout "$REMISE_BIN" p0 \
        "$LOG_NITEMS" "$LEAFSIZE" "$AUTH_FRAC" "$NUM_REQUESTS" \
        > p0.log 2>&1 &
    P0_PID=$!

    # --- wait for P0 to bind, then launch P1 (client) in the foreground ------
    if ! wait_for_listen "$PORT"; then
        echo "error: P0 did not start listening on port $PORT (run $run)" >&2
        echo "----- P0 log -----" >&2
        cat p0.log >&2 || true
        exit 1
    fi

    set +e
    maybe_timeout "$REMISE_BIN" p1 \
        "$LOG_NITEMS" "$LEAFSIZE" "$AUTH_FRAC" "$NUM_REQUESTS" \
        > p1.log 2>&1
    P1_RC=$?
    wait "$P0_PID"
    P0_RC=$?
    set -e
    P0_PID=""

    if [ "$P0_RC" -ne 0 ] || [ "$P1_RC" -ne 0 ]; then
        echo "warning: run $run exited non-zero (P0=$P0_RC, P1=$P1_RC); " \
             "parsed values may be empty" >&2
    fi

    # --- store raw logs ------------------------------------------------------
    {
        echo "--- P0 OUTPUT ---"; cat p0.log
        echo ""
        echo "--- P1 OUTPUT ---"; cat p1.log
        echo ""; echo ""
    } >> "$RAWFILE"

    # --- parse statistics from P0's summary ----------------------------------
    AUTH_REQ=$(extract_number          "Authorized requests"              p0.log)
    UNAUTH_REQ=$(extract_number        "Unauthorized requests"            p0.log)
    AVG_EVAL=$(extract_number          "Average eval latency"             p0.log)
    AVG_AUDIT=$(extract_number         "Average audit latency"            p0.log)
    AVG_WRITE=$(extract_number         "Average write latency"            p0.log)
    AVG_PACL_WRITE=$(extract_number    "Average write latency (for PACL)" p0.log)
    AVG_FCW=$(extract_number           "Average FCW exchange latency"     p0.log)
    THROUGHPUT=$(extract_number        "Throughput"                       p0.log)
    GOODPUT=$(extract_number           "Goodput"                          p0.log)
    ONLINE_THROUGHPUT=$(extract_number "Online throughput"                p0.log)
    ONLINE_GOODPUT=$(extract_number    "Online goodput"                   p0.log)
    EVAL_BW=$(extract_number           "Eval bandwidth"                   p0.log)
    AUDIT_BW=$(extract_number          "Audit bandwidth"                  p0.log)
    WRITE_BW=$(extract_number          "Write bandwidth"                  p0.log)
    TOTAL_BW=$(extract_number          "Total bandwidth"                  p0.log)

    # --- write CSV row -------------------------------------------------------
    echo "$run,\
$AUTH_FRAC,\
$NUM_REQUESTS,\
$AUTH_REQ,\
$UNAUTH_REQ,\
$AVG_EVAL,\
$AVG_AUDIT,\
$AVG_WRITE,\
$AVG_PACL_WRITE,\
$AVG_FCW,\
$THROUGHPUT,\
$GOODPUT,\
$ONLINE_THROUGHPUT,\
$ONLINE_GOODPUT,\
$EVAL_BW,\
$AUDIT_BW,\
$WRITE_BW,\
$TOTAL_BW" \
>> "$CSVFILE"

    printf 'run %d: throughput=%s req/s  goodput=%s auth-req/s\n' \
        "$run" "${THROUGHPUT:-NA}" "${GOODPUT:-NA}"

done

# ----------------------------------------------------------------------------
# Summary
# ----------------------------------------------------------------------------

echo ""
echo "======================================"
echo "Finished all runs."
echo "Raw logs : $RAWFILE"
echo "CSV stats: $CSVFILE"
echo "======================================"
echo ""
echo "AGGREGATE STATISTICS"
echo "======================================"

CSVFILE="$CSVFILE" python3 <<'EOF'
import csv, math, os
from statistics import mean

csv_file = os.environ["CSVFILE"]

metrics = [
    "throughput", "goodput", "online_throughput", "online_goodput",
    "avg_eval_ms", "avg_audit_ms", "avg_write_ms",
    "avg_pacl_write_ms", "avg_fcw_exchange_ms",
    "eval_bandwidth", "audit_bandwidth", "write_bandwidth", "total_bandwidth",
]

data = {m: [] for m in metrics}
with open(csv_file, newline="") as f:
    for row in csv.DictReader(f):
        for m in metrics:
            try:
                data[m].append(float(row[m]))
            except (ValueError, KeyError, TypeError):
                pass  # empty/garbage cell -> skip

print()
for m in metrics:
    vals = data[m]
    if not vals:
        print(f"{m:25s}(no data)")
        continue
    n = len(vals)
    avg = mean(vals)
    if n == 1:
        stddev = ci95 = 0.0
    else:
        var = sum((x - avg) ** 2 for x in vals) / (n - 1)
        stddev = math.sqrt(var)
        ci95 = 1.96 * stddev / math.sqrt(n)   # normal approx; small n -> indicative only
    print(f"{m:25s}mean={avg:14.4f}   stddev={stddev:14.4f}   95CI=±{ci95:12.4f}")
EOF