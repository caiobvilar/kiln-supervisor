# ADR 0003 — Unity/CMock for C, GoogleTest reserved for C++

**Status:** accepted — 2026-07-27

C projects (all current STM32 firmware) use Unity for host unit tests, per
`Containerfile.toolchain`'s original stated intent. CMock is vendored
alongside it for the same C projects once a test needs to isolate code from
a HAL/driver dependency (e.g. testing `02-gesture-imu`'s classifier logic
without a real I2C transaction) — not needed yet by `01-tinyml-m0`, whose
`quantize` module has no such dependency, but installed now rather than
revisited per-project. GoogleTest/GoogleMock is installed too, reserved for
any future C++ module (the RPi/ROS2 orchestration idea is the only current
candidate) — it is not used by any project yet and should not be pulled into
a C-only project for the sake of using it.

Both are **vendored/built into the toolchain image at `podman build` time**,
not fetched via CMake `FetchContent` at project-configure time. The running
container has no network (see `AGENTS.md`'s host-environment section and the
README's split-trust model) — `FetchContent` would silently hang or fail the
first time someone runs `verify.sh` on a machine without the exact GitHub
tarball cached, which is a worse failure mode than a slightly heavier image.
GoogleTest is built from source and `cmake --install`ed to `/usr/local` (so
plain `find_package(GTest)` works); Unity and CMock are vendored as source
under `/opt/unity` and `/opt/cmock` (`$UNITY_DIR`/`$CMOCK_DIR`) rather than
pre-built libraries, since both are designed to be compiled directly into
each test binary — including, for Unity, a future on-target build, which a
prebuilt x86_64 archive could never support anyway.

Versions (`GTEST_VERSION=1.15.2`, `UNITY_VERSION=2.7.0`,
`CMOCK_VERSION=2.7.0`) were checked against each project's actual GitHub
releases before being pinned, not recalled from memory — a lower-stakes
version of the same discipline `hw-facts` requires for hardware documents: a
wrong git tag here just fails `podman build` loudly, but there was no reason
to guess when the release list was one API call away.
