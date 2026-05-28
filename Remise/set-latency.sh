#!/bin/bash

# Usage:
#   ./latency.sh 50ms
#   ./latency.sh 100ms

LATENCY=${1:-50ms}

echo "[-] Removing existing tc rules from lo"
tc qdisc del dev lo root 2>/dev/null

echo "[-] Adding latency ${LATENCY} to loopback interface"

tc qdisc add dev lo root netem delay ${LATENCY}

echo "[-] Current tc configuration:"
tc qdisc show dev lo
