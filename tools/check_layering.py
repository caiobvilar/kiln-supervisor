#!/usr/bin/env python3
"""check_layering.py -- enforce the hexagonal-architecture layering rule.

The rule (03-toolchain.md §2): `src/domain/` may not include any hardware
headers. Domain code is pure logic, compiled natively and host-tested. This
script fails CI if a file under src/domain includes anything that is not
itself in src/domain, src/ports, or the standard library.

Usage:
  python tools/check_layering.py
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
DOMAIN = ROOT / "src" / "domain"
PORTS = ROOT / "src" / "ports"

INCLUDE_RE = re.compile(r'^\s*#\s*include\s*[<"]([^">]+)[>"]', re.M)

# Domain may pull in: its own headers, port interfaces, stdlib, and vendor
# libraries that are explicitly hardware-free.
# NOTE: the INCLUDE_RE regex strips angle brackets, so stdlib headers appear
# as bare names (e.g., "stdint.h", "math.h"). Include both bare names and
# the prefix forms for compatibility.
ALLOWED_PREFIXES = (
    "src/", "stdint.h", "math.h", "stdbool.h", "string.h", "stdlib.h",
    "assert.h", "stddef.h", "limits.h", "float.h", "complex.h", "tgmath.h",
    "ctype.h", "wchar.h", "wctype.h", "setjmp.h", "signal.h", "time.h",
    "errno.h", "locale.h", "inttypes.h", "uchar.h",
)


def main() -> int:
    if not DOMAIN.exists():
        print(f"OK: no src/domain (nothing to check)")
        return 0

    errors = []
    for path in sorted(DOMAIN.rglob("*.[ch]")):
        for inc in INCLUDE_RE.findall(path.read_text(errors="replace")):
            allowed = inc.startswith(ALLOWED_PREFIXES)
            if not allowed and (PORTS / inc).exists():
                allowed = True
            if not allowed and (DOMAIN / inc).exists():
                allowed = True
            if not allowed:
                errors.append(f"{path.relative_to(ROOT)}: includes '{inc}' -- not domain/ports/stdlib")

    if errors:
        for e in errors:
            print(f"ERROR: {e}")
        return 1
    print(f"OK: {len(list(DOMAIN.rglob('*.[ch]')))} domain files, no hardware includes")
    return 0


if __name__ == "__main__":
    sys.exit(main())
