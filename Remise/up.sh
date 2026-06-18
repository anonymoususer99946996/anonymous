#!/usr/bin/env bash
# up.sh — build the image (if needed) and start the p0/p1 containers.
# The image build (see Dockerfile) compiles BOTH binaries in dependency order:
#   1. make remise
#   2. cd prg-bitsliced && make   (generates lowmc/constants_b128_r29_s11.h)
#   3. make remise_sabre          (needs that constants header + -Iprg-bitsliced)
# Containers run `sleep infinity`, so they stay alive until you run down.sh;
# experiments are launched into them by run.sh / run_remise_sabre.sh.
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
  echo "==> Building remise:latest (remise + prg-bitsliced + remise_sabre)"
  docker build --network=host -t remise:latest .
else
  echo "==> Using existing remise:latest (pass --rebuild to force)"
fi

# Verify both benchmark binaries were produced by the build (per the Makefile).
echo "==> Verifying binaries in image"
for bin in remise remise_sabre; do
  if docker run --rm remise:latest test -x "/app/$bin"; then
    echo "    ok: /app/$bin"
  else
    echo "    ERROR: /app/$bin missing from image — check the Dockerfile build steps." >&2
    exit 1
  fi
done

echo "==> Starting containers"
docker compose up -d 2>/dev/null || docker-compose up -d

echo "==> Status"
docker compose ps 2>/dev/null || docker-compose ps

echo
echo "Containers are up. Run an experiment with:"
echo "    ./run.sh <RTT_ms> [bw] [log_nitems] [leafsize] [auth_frac] [num_requests]              # remise"
echo "    ./run_remise_sabre.sh <RTT_ms> [bw] [log_nitems] [leafsize] [auth_frac] [num_requests] # remise_sabre"
echo "Tear down with:  ./down.sh"
