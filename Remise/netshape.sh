#!/usr/bin/env bash
# netshape.sh — apply/clear netem latency (+optional bandwidth) on the
# remise p0/p1 containers. Latency is split across both containers so the
# value you pass is the round-trip time (RTT).
#
# Usage:
#   ./netshape.sh set <RTT_ms> [bandwidth e.g. 1gbit|100mbit]
#   ./netshape.sh clear
#   ./netshape.sh show
#
# Examples:
#   ./netshape.sh set 20            # 20 ms RTT, no bw limit  (LAN-ish)
#   ./netshape.sh set 80 100mbit    # 80 ms RTT, 100 Mbit/s   (WAN)
#   ./netshape.sh clear

set -euo pipefail

CONTAINERS=(p0 p1)
IFACE=eth0   # default interface inside the container on a single bridge network

usage() { sed -n '2,16p' "$0"; exit 1; }

cmd="${1:-}"

case "$cmd" in
  set)
    rtt_ms="${2:-}"
    bw="${3:-}"
    [[ -z "$rtt_ms" ]] && usage

    # split RTT across the two egress paths -> per-side one-way delay
    oneway=$(awk "BEGIN { printf \"%g\", $rtt_ms / 2 }")

    for c in "${CONTAINERS[@]}"; do
      # remove any existing qdisc first (ignore error if none)
      docker exec "$c" tc qdisc del dev "$IFACE" root 2>/dev/null || true

      if [[ -n "$bw" ]]; then
        # netem for delay + tbf for rate limiting, layered
        docker exec "$c" tc qdisc add dev "$IFACE" root handle 1: netem delay "${oneway}ms"
        docker exec "$c" tc qdisc add dev "$IFACE" parent 1: handle 2: \
          tbf rate "$bw" burst 32kbit latency 400ms
      else
        docker exec "$c" tc qdisc add dev "$IFACE" root netem delay "${oneway}ms"
      fi
      echo "[$c] set one-way delay ${oneway}ms${bw:+, rate $bw} on $IFACE"
    done
    echo "Total RTT ≈ ${rtt_ms} ms${bw:+, bandwidth $bw each direction}"
    ;;

  clear)
    for c in "${CONTAINERS[@]}"; do
      docker exec "$c" tc qdisc del dev "$IFACE" root 2>/dev/null || true
      echo "[$c] cleared shaping on $IFACE"
    done
    ;;

  show)
    for c in "${CONTAINERS[@]}"; do
      echo "=== $c ==="
      docker exec "$c" tc qdisc show dev "$IFACE"
    done
    ;;

  *)
    usage
    ;;
esac
