#!/usr/bin/env bash
# setup-dev.sh -- one-shot dev environment setup for the toolchain container.
set -euo pipefail

cd "$(dirname "$0")/.."

echo "Building toolchain container (rootless Podman)..."
podman build -t workbench-toolchain -f Containerfile.toolchain .

echo "Verifying the host-test preset works end to end..."
cmake --preset host-test
cmake --build --preset host-test
ctest --preset host-test

echo "Dev environment ready. Run tests inside the container:"
echo "  podman run --rm -it --userns=keep-id -v \$PWD:/w:Z -w /w localhost/workbench-toolchain"
