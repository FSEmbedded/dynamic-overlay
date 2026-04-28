# Contributing

## Build

```bash
./scripts/build.sh debug           # cross-compile Debug
./scripts/build.sh release         # cross-compile Release (-Os, LTO)
./scripts/build.sh sanitize        # cross-compile Debug with ASan + UBSan
./scripts/build.sh debug --x509    # with X.509 certificate store
./scripts/build.sh test            # native build + run full test suite
./scripts/build.sh clean
```

`SDK_ROOT` defaults to `/opt/fslc-xwayland/5.15-scarthgap`. Override with:

```bash
SDK_ROOT=/path/to/sdk ./scripts/build.sh debug
```

Cross-compile output: `build/`. Test output: `build_test/`.

## CMake options

See [CMake Options](reference/cmake-options.md) for the full list.

## Tests

Run the full suite:

```bash
./scripts/build.sh test
# or:
cd build_test && ctest --output-on-failure
# run a single test binary:
./build_test/dynamic_overlay_tests --gtest_filter='IniParserTest.*'
```

Tests cover: `ini_parser`, `posix_utils`, `device_parser`, `overlay_config`,
`overlay_description`, `dynamic_mounting` helpers, `error`, `scope_guard`,
`string_utils`.

Not tested (hardware-dependent): `main.cpp`, `mount.cpp`, `u-boot.cpp`,
`persistent_mem_detector.cpp`.

## Coding standard

Targeting C++17.

**Exception to the project-wide rules** for this component:

- **`-fno-exceptions` and `-fno-rtti`** are enforced. All functions return
  `[[nodiscard]] enum class Error : uint8_t` with `noexcept`.
- **No `std::filesystem`** — use POSIX APIs. Saves 50–100 KB on a binary
  that must be < 200 KB stripped.
- **No `<iostream>` or `printf`** — use the `LoggerHandler` API.
- **`[[nodiscard]]`** on every error-returning and `std::optional`-returning
  function. Log the failure site before returning the error code.
- **Error codes are ABI-stable** — `Error` enum values in `src/error.h` are
  explicit integers and append-only. Do not renumber or reuse. Update
  [error-codes.md](reference/error-codes.md) in the same commit.
- **Overlay stacking limit**: Linux caps overlay stacking at 2 levels. Do not
  design code paths that would overlay a path already backed by an overlay
  from the same `dynamic_overlay` run. The runtime enforces this and returns
  `Error::max_overlay_depth` (code 29) if violated.
- **U-Boot writes**: call `addVariable()` then `flushEnvironment()` in a single
  scope. One `flushEnvironment()` per open context — a second call on the same
  context silently fails (libubootenv invalidates the context after store).
