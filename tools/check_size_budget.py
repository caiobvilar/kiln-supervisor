#!/usr/bin/env python3
"""check_size_budget.py -- enforce the flash/RAM size budget gate.

Reads `arm-none-eabi-size -A` output and fails the build above the configured
budget. Budgets are per-project constants set in CMake; the CI step passes them
via --flash-max/--ram-max.

Usage:
  python tools/check_size_budget.py size.txt --flash-max 65536 --ram-max 20480
"""

from __future__ import annotations

import argparse
import sys


def parse_size_output(text: str) -> dict[str, int]:
    """Parse `arm-none-eabi-size -A <elf>` output.

    Sums .text/.data/.rodata into flash, and .data/.bss into RAM. The .data
    section occupies both (loaded image + live copy), so it counts toward both
    budgets -- the honest interpretation.
    """
    flash = 0
    ram = 0
    for line in text.splitlines():
        parts = line.split()
        if len(parts) < 2:
            continue
        section, size = parts[0], parts[1]
        try:
            size = int(size)
        except ValueError:
            continue
        if section in (".text", ".rodata"):
            flash += size
        elif section == ".data":
            flash += size
            ram += size
        elif section == ".bss":
            ram += size
    return {"flash": flash, "ram": ram}


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("sizefile")
    ap.add_argument("--flash-max", type=int, required=True)
    ap.add_argument("--ram-max", type=int, required=True)
    args = ap.parse_args()

    usage = parse_size_output(open(args.sizefile).read())
    ok = True
    for kind, limit in (("flash", args.flash_max), ("ram", args.ram_max)):
        used = usage[kind]
        pct = 100.0 * used / limit if limit else 0.0
        marker = "OK" if pct <= 100.0 else "OVER"
        if pct > 100.0:
            ok = False
        print(f"{kind:5s} {used:8d} / {limit:8d}  {pct:6.1f}%  {marker}")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
