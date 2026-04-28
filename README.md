# Dynamic Overlay

Preinit overlay filesystem mounting for embedded Linux A/B update systems.

## Overview

Runs before any init system. Detects boot storage, mounts the active application
squashfs image (A/B slot selection via U-Boot), and applies overlay filesystems
defined in `overlay.ini`.

See [Boot Sequence](docs/diagrams/boot_sequence.md) for the full
step-by-step flow.

## Build

```bash
# Cross-compile (SDK_ROOT defaults to /opt/fslc-xwayland/5.15-scarthgap)
./scripts/build.sh debug          # Debug build
./scripts/build.sh release        # Release build (-Os, LTO)
./scripts/build.sh sanitize       # Debug build with ASan + UBSan
./scripts/build.sh debug --x509   # With X.509 certificate store

# Native test build + run full gtest suite
./scripts/build.sh test

# Wipe all build directories
./scripts/build.sh clean

# Override SDK location
SDK_ROOT=/path/to/sdk ./scripts/build.sh debug
```

Test sources live in `tests/` — see `tests/CMakeLists.txt` for the
currently registered cases.

### Dependencies

| Library | Purpose | Required |
|---------|---------|----------|
| libubootenv | U-Boot environment access | Yes |
| libblkid | Block device identification | Yes |
| libjsoncpp | JSON parsing (cert store) | Only with `--x509` |
| libarchive | Archive extraction (cert store) | Only with `--x509` |

## Configuration

### overlay.ini

Located inside the mounted application image. Defines which directories
are overlaid onto the root filesystem.

```ini
[ApplicationFolder]
entry1=/etc
entry2=/usr/bin

[PersistentMemory.etc]
lowerdir=/rw_fs/root/application/current/etc
upperdir=/rw_fs/root/upperdir/etc
workdir=/rw_fs/root/workdir/etc
mergedir=/etc
```

- **ApplicationFolder** entries create read-only overlays (app content over system)
- **PersistentMemory** sections create read-write overlays (changes survive reboots)

Full reference: [overlay.ini Reference](docs/reference/overlay-ini.md)

### U-Boot Variables

| Variable | Values | Description |
|----------|--------|-------------|
| `application` | A, B | Active application slot |
| `BOOT_ORDER` | "A B", "B A" | Boot priority |
| `BOOT_A_LEFT` / `BOOT_B_LEFT` | 0-3 | Remaining boot attempts |
| `update_reboot_state` | 0-12 | Update state machine position |

## Documentation

| Document | Content |
|----------|---------|
| [Architecture](docs/architecture.md) | Module design, error handling, build config |
| [overlay.ini Reference](docs/reference/overlay-ini.md) | Configuration file format |
| [Boot Sequence](docs/diagrams/boot_sequence.md) | Step-by-step preinit flow |
| [Component Architecture](docs/diagrams/component_architecture.md) | Module dependency diagram |
| [Mount Sequence](docs/diagrams/mount_sequence.md) | Overlay processing flow |
| [Directory Layout](docs/diagrams/directory_layout.md) | Filesystem tree |

## Troubleshooting

Log messages below are the `LOG_ERROR` strings emitted to `/dev/kmsg`
(or `stderr` in test builds) — grep `dmesg` for these substrings.

**`could not determine memory type (NAND|eMMC)`**
- `PersistentMemDetector::create` could not resolve the boot device via
  `/sys/bdinfo/boot_dev` or `/proc/cmdline root=`
- Verify the vendor sysfs or kernel cmdline on this board

**`application image not found: <path>`**
- Selected squashfs missing at `/rw_fs/root/application/app_[a|b].squashfs`
- Check the U-Boot `application` variable and the A/B fallback path

**Kernel: `maximum fs stacking depth exceeded` (EBUSY on overlay mount)**
- Linux caps overlay stacking at 2 levels
- Reduce nested overlays; do not overlay a path that is itself backed by
  an overlay

### Debug Logging

```bash
./scripts/build.sh debug                        # Standard logging
# Add -DENABLE_DEBUG_LOGGING=ON for verbose output
```

Output goes to `/dev/kmsg` during preinit, `stderr` in test builds.

## Related Components

| Component | Purpose |
|-----------|---------|
| [fs-updater-lib](https://github.com/fsembedded/fs-updater-lib/blob/main/README.md) | Core update library; writes the U-Boot state variables this binary reads at boot |
| [fs-updater-cli](https://github.com/fsembedded/fs-updater-cli/blob/main/README.md) | CLI frontend for `fs-updater-lib`; triggers the update cycles that change A/B slot state |

## License

MIT License
