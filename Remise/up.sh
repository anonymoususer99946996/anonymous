#!/usr/bin/env bash
# up.sh — build the image (if needed) and start the p0/p1 containers.
# Containers run `sleep infinity`, so they stay alive until you run down.sh;
# experiments are launched into them by run.sh.
#
# Usage:
#   ./up.sh            # build if image missing, then start
#   ./up.sh --rebuild  # force a rebuild even if the image exists

set -euo pipefail
cd "$(dirname "$0")"

REBUILD=0
[[ "${1:-}" == "--rebuild" ]] && REBUILD=1

mkdir -p results

# Build the image if it doesn't exist, or if --rebuild was requested.
# --network=host is used because the build pulls apt packages and some
# networks intercept Docker's default build network.
if [[ "$REBUILD" -eq 1 ]] || ! docker image inspect remise:latest >/dev/null 2>&1; then
  echo "==> Building remise:latest"
  docker build --network=host -t remise:latest .
else
  echo "==> Using existing remise:latest (pass --rebuild to force)"
fi

echo "==> Starting containers"
docker compose up -d 2>/dev/null || docker-compose up -d

echo "==> Status"
docker compose ps 2>/dev/null || docker-compose ps

echo
echo "Containers are up. Run an experiment with:  ./run.sh <RTT_ms> [bw] [args...]"
echo "Tear down with:  ./down.sh"

