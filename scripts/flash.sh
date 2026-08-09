#!/usr/bin/env bash
# flash.sh -- flash the built firmware to the board via probe-rs.
set -euo pipefail

cd "$(dirname "$0")/.."

CHIP="${CHIP:-STM32F4}"
ELF="${1:-build/target-debug/firmware.elf}"

if [[ ! -f "$ELF" ]]; then
    echo "firmware not built: $ELF (run: cmake --preset target-debug && cmake --build --preset target-debug)" >&2
    exit 1
fi

probe-rs download --chip "$CHIP" "$ELF"
probe-rs run --chip "$CHIP" "$ELF"
