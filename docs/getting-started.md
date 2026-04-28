# Getting Started

## Prerequisites

- SDK sourced: `/opt/fslc-xwayland/5.15-scarthgap/environment-setup-cortexa53-fslc-linux`
- Target device with U-Boot environment configured for A/B slots
- `libubootenv` and `libblkid` available in the SDK sysroot

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

Cross-compile output lands in `build/`. Test build output lands in `build_test/`.

## Install

```bash
scp build/dynamic_overlay root@<device>:/usr/sbin/dynamic_overlay
```

The binary is installed to `/usr/sbin/dynamic_overlay` and invoked early in
the boot process, before the init system starts.

## Run on device

`dynamic_overlay` requires no arguments. The boot device, U-Boot variables,
and overlay paths are all read at runtime:

```bash
/usr/sbin/dynamic_overlay
echo $?   # 0 = success; non-zero = error code (see reference/error-codes.md)
```

In production, the binary is called by the bootloader init sequence before
`switch_root`. In development, it can be run manually from an early shell to
test overlay configuration without a full reboot.

## Enable debug logging

Add `-DENABLE_DEBUG_LOGGING=ON` to your build flags, or rebuild with:

```bash
SDK_ROOT=/path/to/sdk cmake -DENABLE_DEBUG_LOGGING=ON ../..
make
```

Log output goes to `/dev/kmsg` in production (`LOG_BACKEND=KMSG`) and to
`stderr` in test builds (`LOG_BACKEND=STDERR`).

## Run the test suite

```bash
./scripts/build.sh test
# or directly:
cd build_test && ctest --output-on-failure
# run one test binary:
./build_test/dynamic_overlay_tests --gtest_filter='IniParserTest.*'
```

Tests cover: `ini_parser`, `posix_utils`, `device_parser`, `overlay_config`,
`overlay_description`, `dynamic_mounting` helpers, `error`, `scope_guard`,
`string_utils`.

## Next steps

- [Architecture](architecture.md) — module diagram and boot flow
- [overlay.ini Reference](reference/overlay-ini.md) — configuration file format
- [Boot Sequence](integration/boot-sequence.md) — full step-by-step mount sequence
- [Yocto Integration](integration/yocto.md) — partition layout, mtdparts, fw_env.config
- [Error Codes](reference/error-codes.md) — exit codes (ABI-stable)
