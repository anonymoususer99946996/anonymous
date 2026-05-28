#!/bin/bash

# ============================================================
# MASTER EXPERIMENT DRIVER (CONFIG FILE VERSION)
#
# Usage:
#   ./run_all_experiments.sh configs.txt
#
# Config file format:
#
#   # comments allowed
#   LOG_NITEMS LEAFSIZE AUTH_FRAC NUM_REQUESTS NUM_RUNS
#
# Example:
#   20 64 1.0 100 5
#   20 64 0.5 100 5
#   24 64 0.1 100 5
# ============================================================

if [ "$#" -ne 1 ]; then
    echo "Usage:"
    echo "./run_all_experiments.sh <config_file>"
    exit 1
fi

CONFIG_FILE=$1

if [ ! -f "$CONFIG_FILE" ]; then
    echo "Config file not found: $CONFIG_FILE"
    exit 1
fi

OUTDIR="results"
mkdir -p "$OUTDIR"

TIMESTAMP=$(date +"%Y%m%d_%H%M%S")

MASTERFILE="$OUTDIR/aggregate_${TIMESTAMP}.txt"

echo "Writing aggregate results to:"
echo "$MASTERFILE"

# ============================================================
# READ CONFIG FILE
# ============================================================

while read -r LOG_NITEMS LEAFSIZE AUTH_FRAC NUM_REQUESTS NUM_RUNS
do

    # Skip comments and empty lines

    [[ -z "$LOG_NITEMS" ]] && continue
    [[ "$LOG_NITEMS" =~ ^# ]] && continue

    echo ""
    echo "===================================================" | tee -a "$MASTERFILE"
    echo "RUN CONFIGURATION"                                  | tee -a "$MASTERFILE"
    echo "===================================================" | tee -a "$MASTERFILE"

    echo "log_nitems        = $LOG_NITEMS"   | tee -a "$MASTERFILE"
    echo "leafsize          = $LEAFSIZE"     | tee -a "$MASTERFILE"
    echo "authorized_frac   = $AUTH_FRAC"    | tee -a "$MASTERFILE"
    echo "num_requests      = $NUM_REQUESTS" | tee -a "$MASTERFILE"
    echo "num_runs          = $NUM_RUNS"     | tee -a "$MASTERFILE"

    echo "" | tee -a "$MASTERFILE"

    # ========================================================
    # RUN EXPERIMENT
    # ========================================================

    OUTPUT=$(
        ./run_experiment.sh \
            "$LOG_NITEMS" \
            "$LEAFSIZE" \
            "$AUTH_FRAC" \
            "$NUM_REQUESTS" \
            "$NUM_RUNS"
    )

    # ========================================================
    # EXTRACT AGGREGATE STATISTICS
    # ========================================================

    echo "$OUTPUT" \
    | awk '
        /AGGREGATE STATISTICS/ {flag=1; next}
        /^$/ && flag {next}
        flag
    ' \
    | tee -a "$MASTERFILE"

    echo "" | tee -a "$MASTERFILE"
    echo "" | tee -a "$MASTERFILE"

done < "$CONFIG_FILE"

echo "===================================================" | tee -a "$MASTERFILE"
echo "ALL EXPERIMENTS FINISHED"                           | tee -a "$MASTERFILE"
echo "===================================================" | tee -a "$MASTERFILE"

echo ""
echo "Master aggregate file:"
echo "$MASTERFILE"
