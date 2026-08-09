# Software/System Requirements Specification — embedded-template

| | |
|---|---|
| Project | embedded-template (reusable infra repo) |
| Version | 1.0 |
| Status | approved |

*This SRS describes the template repository itself. Every project repo created
from this template **replaces** this file (and `docs/requirements/`) with its
own SRS before writing code — program rule 1 (requirements first). The content
below exists so the template is green from the first commit and demonstrates
the full requirements pipeline (`docs/02-srs.md` → YAML → `tools/gen_rtm.py` →
`docs/06-rtm.md`).*

## 1. Purpose and scope

The template shall provide the reusable engineering infrastructure every
project repo shares: CMake toolchain presets, CI pipelines, requirements
tooling, fake HAL, and doc skeletons, so a new project starts already-green.

## 2. Stakeholders and needs

| Need | Stakeholder | Need text |
|---|---|---|
| N-01 | Owner | Every project repo shall be host-testable from the first commit. |

## 3. Definitions and abbreviations

- **L1 gate** — host (native) unit-test gate; see the portfolio process.
- **RTM** — Requirements Traceability Matrix (`docs/06-rtm.md`, generated).

## 4. System context

The template is a source template consumed by `gh repo create --template`.
It contains no firmware; it is the platform the firmware repos build on.

## 5. Assumptions and constraints

- Host tests run on Linux; the toolchain container (rootless Podman) is the
  supported way to run them.
- CI runs on GitHub Actions with a self-hosted runner for HIL.

## 6. Requirements

### 6.1 Functional

1. **TPL-FUN-001** (shall) — The domain placeholder shall return the value 42
   when invoked. The placeholder exists so the L1 pipeline has something to
   test from the first commit. (Source of truth: `docs/requirements/template.yaml`.)

## 7. Verification summary

| Method | Count |
|---|---|
| Inspection | 0 |
| Analysis | 0 |
| Demonstration | 0 |
| Test | 1 |

*Full traceability is in the generated `docs/06-rtm.md`.*

## 8. Open issues

None.

## 9. Change log

| Version | Date | Change | Commit |
|---|---|---|---|
| 1.0 | | Initial baseline | |
