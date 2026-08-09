#!/usr/bin/env python3
"""gen_rtm.py -- requirements <-> tests traceability generator and gate.

Source of truth: YAML requirements under docs/requirements/. This script:

  * `gen`   -> renders docs/06-rtm.md (traceability matrix, Markdown tables)
  * `check` -> fails (exit 1) on any of the CI gates below; used in ci.yml

Gates (from 14-template-SRS.md, "CI gates on the YAML"):

  1. Every `priority: shall` + `status: approved` requirement has >=1 test_case.
  2. Every referenced test_case ID exists in the test suite (matched against
     `@verifies <ID>` tags in test sources).
  3. Every `@verifies` tag in a test references a requirement ID that exists.
  4. No duplicate IDs; no ID reused after being marked deleted.
  5. Every requirement has a `parent` resolving to a need (N-nn) or another
     requirement ID.
  6. Every requirement has non-empty `rationale` and a `verification` method.
  7. No requirement text contains a banned word (see SRS template).
  8. The SRS verification-count table matches generated totals (checked by
     parsing docs/02-srs.md's "## 7. Verification summary" table).

Usage:
  python tools/gen_rtm.py gen   [--out docs/06-rtm.md]
  python tools/gen_rtm.py check
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

import yaml

ROOT = Path(__file__).resolve().parent.parent
REQ_DIR = ROOT / "docs" / "requirements"
RTM_OUT = ROOT / "docs" / "06-rtm.md"
SRS = ROOT / "docs" / "02-srs.md"
TEST_DIRS = [ROOT / "test"]

BANNED = [
    "fast", "slow", "robust", "efficient", "user-friendly", "flexible",
    "optimal", "reasonable", "appropriate", "as needed", "if possible",
    "etc.", "and/or", "support", "handle", "process",
]
BANNED_RE = re.compile(
    r"\b(" + "|".join(re.escape(w) for w in BANNED) + r")\b", re.IGNORECASE
)

VALID_VERIFICATION = {"test", "analysis", "demonstration", "inspection"}


def load_requirements() -> list[dict]:
    """Load and flatten all YAML requirement files."""
    if not REQ_DIR.exists():
        return []
    reqs: list[dict] = []
    for path in sorted(REQ_DIR.glob("*.yaml")):
        data = yaml.safe_load(path.read_text())
        if not data:
            continue
        for item in data:
            item["_file"] = path.name
            reqs.append(item)
    return reqs


def load_test_verifies() -> dict[str, set[str]]:
    """Scan test sources for `@verifies <ID>` tags.

    Returns {test_name: {req_id, ...}}. A "test" is the file path relative to
    ROOT -- the granularity used in the requirement's test_case list.
    """
    result: dict[str, set[str]] = {}
    for tdir in TEST_DIRS:
        if not tdir.exists():
            continue
        for path in tdir.rglob("*"):
            if path.suffix not in {".c", ".cpp", ".py", ".robot", ".rs"}:
                continue
            text = path.read_text(errors="replace")
            ids = re.findall(r"@verifies\s+([\w-]+)", text)
            if ids:
                rel = path.relative_to(ROOT).as_posix()
                result[rel] = set(ids)
    return result


def check(reqs: list[dict]) -> list[str]:
    errors: list[str] = []
    verifies = load_test_verifies()
    existing_tests = set(verifies.keys())
    req_by_id: dict[str, dict] = {}
    deleted_ids: set[str] = set()

    for r in reqs:
        rid = r.get("id")
        if not rid:
            errors.append(f"[{r.get('_file')}] requirement without id")
            continue
        if rid in req_by_id:
            errors.append(f"duplicate requirement id: {rid}")
        if rid in deleted_ids:
            errors.append(f"id reused after being marked deleted: {rid}")
        if r.get("status") == "deleted":
            deleted_ids.add(rid)
        req_by_id[rid] = r

    active = [r for r in reqs if r.get("status") != "deleted"]

    for r in active:
        rid = r.get("id", "?")
        text = r.get("text", "")
        status = r.get("status", "draft")
        priority = r.get("priority", "should")
        verif = r.get("verification", "")
        parent = r.get("parent", "")
        cases = r.get("test_case", []) or []

        # Gate 1
        if priority == "shall" and status == "approved" and not cases:
            errors.append(f"[{rid}] approved 'shall' has no test_case")
        # Gate 5
        if not parent or not _parent_resolves(parent, req_by_id):
            errors.append(f"[{rid}] parent '{parent}' does not resolve to a need or requirement")
        # Gate 6
        if not str(r.get("rationale", "")).strip():
            errors.append(f"[{rid}] empty rationale")
        if verif not in VALID_VERIFICATION:
            errors.append(f"[{rid}] verification '{verif}' not in {sorted(VALID_VERIFICATION)}")
        # Gate 7
        for match in BANNED_RE.finditer(text):
            errors.append(f"[{rid}] banned word in requirement text: '{match.group(1)}'")
        # Gate 2
        for case in cases:
            if case not in existing_tests:
                errors.append(f"[{rid}] test_case '{case}' not found in test suite")

    # Gate 3
    for test, ids in sorted(verifies.items()):
        for rid in ids:
            if rid not in req_by_id:
                errors.append(f"[{test}] @verifies unknown requirement id '{rid}'")

    return errors


def _parent_resolves(parent: str, req_by_id: dict[str, dict]) -> bool:
    return bool(re.fullmatch(r"N-\d+", parent)) or parent in req_by_id


def render(reqs: list[dict]) -> str:
    active = [r for r in reqs if r.get("status") != "deleted"]
    by_type: dict[str, list[dict]] = {}
    for r in active:
        by_type.setdefault(r.get("type", "other"), []).append(r)

    order = ["functional", "performance", "interface", "electrical",
             "safety", "reliability", "constraint", "other"]

    lines = [
        "# Requirements Traceability Matrix",
        "",
        "_Generated by `tools/gen_rtm.py gen` — do not hand-edit._",
        "",
        f"Total requirements: {len(active)} · deleted: {len(reqs) - len(active)}",
        "",
        "| ID | Type | Priority | Status | Verification | Test cases | Text |",
        "|---|---|---|---|---|---|---|",
    ]
    for t in order + [k for k in by_type if k not in order]:
        for r in by_type.get(t, []):
            cases = ", ".join(r.get("test_case", []) or []) or "—"
            text = (r.get("text") or "").replace("|", "\\|")
            lines.append(
                f"| {r.get('id','?')} | {t} | {r.get('priority','')} | "
                f"{r.get('status','')} | {r.get('verification','')} | "
                f"{cases} | {text} |"
            )
    lines.append("")
    return "\n".join(lines)


def check_srs_verification_counts(reqs: list[dict]) -> list[str]:
    """Gate 8: SRS '## 7. Verification summary' counts match generated totals."""
    if not SRS.exists():
        return ["docs/02-srs.md missing -- cannot check verification counts"]
    text = SRS.read_text(errors="replace")
    m = re.search(r"## 7\. Verification summary(.*?)(?=\n## |\Z)", text, re.S)
    if not m:
        return ["docs/02-srs.md has no '## 7. Verification summary' section"]
    counts: dict[str, int] = {}
    for row in re.finditer(r"\|\s*(\w+)\s*\|\s*(\d+)", m.group(1)):
        counts[row.group(1).lower()] = int(row.group(2))

    actual: dict[str, int] = {}
    for r in reqs:
        if r.get("status") == "deleted":
            continue
        v = r.get("verification", "")
        actual[v] = actual.get(v, 0) + 1

    errors = []
    for method, expected in sorted(counts.items()):
        got = actual.get(method, 0)
        if got != expected:
            errors.append(
                f"verification count mismatch: SRS says {method}={expected}, "
                f"YAML has {got}"
            )
    return errors


def main() -> int:
    ap = argparse.ArgumentParser(description="requirements <-> tests RTM gate")
    ap.add_argument("mode", choices=["gen", "check"])
    ap.add_argument("--out", default=str(RTM_OUT))
    args = ap.parse_args()

    reqs = load_requirements()
    errors = check(reqs)

    if args.mode == "gen":
        RTM_OUT.write_text(render(reqs))
        print(f"wrote {RTM_OUT} ({len(reqs)} requirements)")
        return 0 if not errors else (print("\n".join(errors)), 1)[1]

    errors += check_srs_verification_counts(reqs)
    if errors:
        for e in errors:
            print(f"ERROR: {e}")
        return 1
    print(f"OK: {len(reqs)} requirements, {len(errors)} violations")
    return 0


if __name__ == "__main__":
    sys.exit(main())
