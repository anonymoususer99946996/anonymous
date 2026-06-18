#!/usr/bin/env bash
# down.sh — stop and remove the p0/p1 containers and the remise network.
# The image (remise:latest) is kept so the next up.sh is fast.
#
# Usage:
#   ./down.sh           # stop + remove containers/network
#   ./down.sh --rmi     # also remove the remise:latest image

set -euo pipefail
cd "$(dirname "$0")"

echo "==> Clearing any network shaping (best effort)"
./netshape.sh clear 2>/dev/null || true

echo "==> Killing containers (SIGKILL; sleep infinity ignores graceful stop)"
docker kill p0 p1 2>/dev/null || sudo docker kill p0 p1 2>/dev/null || true

echo "==> Removing containers and network"
docker compose down 2>/dev/null || docker-compose down

if [[ "${1:-}" == "--rmi" ]]; then
  echo "==> Removing image remise:latest"
  docker image rm remise:latest 2>/dev/null || true
fi

echo "==> Done."
