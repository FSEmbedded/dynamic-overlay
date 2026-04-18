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
./scripts/build.sh debug --x509   # With X.509 certificate store

# Native test build
./scripts/build.sh test           # Build and run 107 unit tests

# Override SDK location
SDK_ROOT=/path/to/sdk ./scripts/build.sh debug
```

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

Full reference: [overlay.ini Reference](docs/overlay_ini_reference.md)

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
| [overlay.ini Reference](docs/overlay_ini_reference.md) | Configuration file format |
| [Boot Sequence](docs/diagrams/boot_sequence.md) | Step-by-step preinit flow |
| [Component Architecture](docs/diagrams/component_architecture.md) | Module dependency diagram |
| [Mount Sequence](docs/diagrams/mount_sequence.md) | Overlay processing flow |
| [Directory Layout](docs/diagrams/directory_layout.md) | Filesystem tree |

## Troubleshooting

**"Could not determine current memory type"**
- Check `/sys/bdinfo/boot_dev` or `/proc/cmdline` contains valid boot device info

**"Application image not found"**
- Verify squashfs exists at `/rw_fs/root/application/app_[a|b].squashfs`
- Check U-Boot `application` variable

**"Maximum fs stacking depth exceeded"**
- Linux kernel limits overlay stacking to 2 levels
- Reduce nested overlays in configuration

### Debug Logging

```bash
./scripts/build.sh debug                        # Standard logging
# Add -DENABLE_DEBUG_LOGGING=ON for verbose output
```

Output goes to `/dev/kmsg` during preinit, `stderr` in test builds.

## License

MIT License
