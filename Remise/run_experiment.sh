#!/bin/bash

if [ "$#" -ne 5 ]; then
    echo "Usage:"
    echo "./run_experiment.sh <log_nitems> <leafsize> <authorized_fraction> <num_requests> <num_runs>"
    exit 1
fi

LOG_NITEMS=$1
LEAFSIZE=$2
AUTH_FRAC=$3
NUM_REQUESTS=$4
NUM_RUNS=$5

OUTDIR="results"
mkdir -p "$OUTDIR"

TIMESTAMP=$(date +"%Y%m%d_%H%M%S")

RAWFILE="$OUTDIR/raw_${TIMESTAMP}.log"
CSVFILE="$OUTDIR/stats_${TIMESTAMP}.csv"

echo "Writing raw logs to: $RAWFILE"
echo "Writing CSV stats to: $CSVFILE"

# ============================================================
# CSV HEADER
# ============================================================

echo "run,\
authorized_fraction,\
total_requests,\
authorized_requests,\
cover_requests,\
avg_eval_ms,\
avg_audit_ms,\
avg_write_ms,\
throughput,\
goodput,\
eval_bandwidth,\
audit_bandwidth,\
write_bandwidth,\
total_bandwidth" \
> "$CSVFILE"

# ============================================================
# EXPERIMENT LOOP
# ============================================================

for ((run=1; run<=NUM_RUNS; run++))
do

    echo "======================================" | tee -a "$RAWFILE"
    echo "RUN $run / $NUM_RUNS"                 | tee -a "$RAWFILE"
    echo "======================================" | tee -a "$RAWFILE"

    ./remise p0 \
        "$LOG_NITEMS" \
        "$LEAFSIZE" \
        "$AUTH_FRAC" \
        "$NUM_REQUESTS" \
        > p0.log 2>&1 &

    P0_PID=$!

    sleep 1

    ./remise p1 \
        "$LOG_NITEMS" \
        "$LEAFSIZE" \
        "$AUTH_FRAC" \
        "$NUM_REQUESTS" \
        > p1.log 2>&1

    wait "$P0_PID"

    # ========================================================
    # STORE RAW LOGS
    # ========================================================

    echo "--- P0 OUTPUT ---" >> "$RAWFILE"
    cat p0.log >> "$RAWFILE"

    echo "" >> "$RAWFILE"

    echo "--- P1 OUTPUT ---" >> "$RAWFILE"
    cat p1.log >> "$RAWFILE"

    echo "" >> "$RAWFILE"
    echo "" >> "$RAWFILE"

    # ========================================================
    # PARSE STATISTICS FROM P0
    # ========================================================

    AUTH_REQ=$(grep "Authorized requests" p0.log | awk '{print $4}')

    COVER_REQ=$(grep "Cover requests" p0.log | awk '{print $4}')

    AVG_EVAL=$(grep "Average eval latency" p0.log | awk '{print $5}')

    AVG_AUDIT=$(grep "Average audit latency" p0.log | awk '{print $5}')

    AVG_WRITE=$(grep "Average write latency" p0.log | awk '{print $5}')

    THROUGHPUT=$(grep "^Throughput" p0.log | awk '{print $3}')

    GOODPUT=$(grep "^Goodput" p0.log | awk '{print $3}')

    EVAL_BW=$(grep "Eval bandwidth" p0.log | awk '{print $4}')

    AUDIT_BW=$(grep "Audit bandwidth" p0.log | awk '{print $4}')

    WRITE_BW=$(grep "Write bandwidth" p0.log | awk '{print $4}')

    TOTAL_BW=$(grep "Total bandwidth" p0.log | awk '{print $4}')

    # ========================================================
    # WRITE CSV ROW
    # ========================================================

    echo "$run,\
$AUTH_FRAC,\
$NUM_REQUESTS,\
$AUTH_REQ,\
$COVER_REQ,\
$AVG_EVAL,\
$AVG_AUDIT,\
$AVG_WRITE,\
$THROUGHPUT,\
$GOODPUT,\
$EVAL_BW,\
$AUDIT_BW,\
$WRITE_BW,\
$TOTAL_BW" \
>> "$CSVFILE"

done

# ============================================================
# SUMMARY
# ============================================================

echo ""
echo "======================================"
echo "Finished all runs."
echo "======================================"
echo "Raw logs : $RAWFILE"
echo "CSV stats: $CSVFILE"
echo "======================================"

echo ""
echo "======================================"
echo "AGGREGATE STATISTICS"
echo "======================================"

python3 <<EOF

import csv
import math

csv_file = "$CSVFILE"

metrics = [
    "throughput",
    "goodput",
    "avg_eval_ms",
    "avg_audit_ms",
    "avg_write_ms",
    "eval_bandwidth",
    "audit_bandwidth",
    "write_bandwidth",
    "total_bandwidth"
]

data = {m: [] for m in metrics}

with open(csv_file, newline="") as f:

    reader = csv.DictReader(f)

    for row in reader:

        for m in metrics:
            data[m].append(float(row[m]))

for m in metrics:

    vals = data[m]

    n = len(vals)

    mean = sum(vals) / n

    variance = sum(
        (x - mean) ** 2 for x in vals
    ) / (n - 1)

    stddev = math.sqrt(variance)

    ci95 = 1.96 * stddev / math.sqrt(n)

    print(
        f"{m:20s} "
        f"mean = {mean:.4f}   "
        f"stddev = {stddev:.4f}   "
        f"95CI = ±{ci95:.4f}"
    )

EOF