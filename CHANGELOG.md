# Changelog

All notable changes to this repository's template/landing state are recorded
here per project repos' CHANGELOG convention (generated from Conventional
Commits on tag).

## [Unreleased]

- Fix: over-length replies return ERR_RESPONSE with residual bytes drained (no cross-transaction contamination); spec wording aligned.
- Milestone: generic Modbus RTU client (fc 03/06/16) with bounded response timeout over the UART port seam — KILN-FUN-004 approved; register-constants header added (unverified); fc16 framing helper limitation recorded.
- feat: add host-testable Modbus RTU framing + CRC-16 domain layer (fc 03/06/16, KILN-FUN-003)
- Initial template state: CMake toolchain files, CI pipelines, requirements
  tooling, fake HAL, doc skeletons.
