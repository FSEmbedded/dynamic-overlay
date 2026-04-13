# Dynamic Overlay

Early-boot overlay filesystem mounting tool for F&S embedded Linux systems with A/B update support.

## Overview

Dynamic Overlay is a preinit-stage tool that mounts overlay filesystems before systemd or any other init system starts. It is part of the F&S Update Framework and handles:

- Automatic detection of boot memory type (eMMC or NAND)
- A/B slot selection based on U-Boot environment variables
- SquashFS application image mounting
- Overlay filesystem configuration from `overlay.ini`
- Persistent and read-only overlay mounts

## Features

- **A/B Update Support**: Automatically selects the correct application image (app_a.squashfs or app_b.squashfs) based on U-Boot variables
- **Multi-Memory Support**: Detects and handles both eMMC and NAND flash storage
- **Flexible Overlay Configuration**: Supports both read-only (ApplicationFolder) and read-write (PersistentMemory) overlays
- **Automatic Rollback Detection**: Detects failed firmware updates and handles rollback scenarios
- **X.509 Certificate Store** (optional): Extracts and mounts certificate stores for Azure ADU support

## Build Requirements

### Dependencies

| Library | Purpose |
|---------|---------|
| libinicpp | INI file parsing |
| libubootenv | U-Boot environment access |
| libblkid | Block device identification |
| libjsoncpp | JSON parsing (for X.509 feature) |
| zlib | Compression support |

### CMake Configuration

```bash
mkdir build && cd build
cmake .. \
    -DRAUC_SYSTEM_CONF_PATH=/etc/rauc/system.conf \
    -DNAND_RAUC_SYSTEM_CONF_PATH=/etc/rauc/system.conf.nand \
    -DEMMC_RAUC_SYSTEM_CONF_PATH=/etc/rauc/system.conf.mmc \
    -DUBOOT_ENV_PATH=/etc/fw_env.config \
    -DEMMC_UBOOT_ENV_PATH=/etc/fw_env.config.mmc \
    -DNAND_UBOOT_ENV_PATH=/etc/fw_env.config.nand
make
```

### Optional Features

#### X.509 Certificate Store Mount

Enable Azure ADU certificate store support:

```bash
cmake .. \
    -DBUILD_X509_CERTIFICATE_STORE_MOUNT=ON \
    -DTARGET_ARCHIV_DIR_PATH=/path/to/archive \
    -DTARGET_ADU_DIR_PATH=/adu \
    -DSOURCE_ARCHIVE_MTD_FILE_PATH=/path/to/mtd/archive \
    -DSOURCE_ARCHIVE_MMC_FILE_PATH=/path/to/mmc/archive \
    -DFUS_AZURE_CONFIGURATION=/path/to/config \
    -DFUS_AZURE_CERT_CERTIFICATE_NAME=cert.pem \
    -DFUS_AZURE_CERT_KEY_NAME=key.pem \
    -DPART_NAME_MTD_CERT=cert_partition
```

## Installation

```bash
make install
```

The binary is installed to `${CMAKE_INSTALL_SBINDIR}` (typically `/usr/sbin/`).

## Configuration

### overlay.ini Format

The overlay configuration file is located at `/rw_fs/root/application/current/overlay.ini` (inside the mounted application image).

#### ApplicationFolder Section

Defines directories from the application image to overlay onto the root filesystem (read-only).

```ini
[ApplicationFolder]
entry1=/etc
entry2=/usr/bin
entry3=/opt/app
```

Each entry specifies a path that will be overlaid with content from the application image. The overlay is read-only with the application content taking precedence over system files.

#### PersistentMemory Sections

Defines directories that need persistent, writable overlay storage.

```ini
[PersistentMemory.etc]
lowerdir=/rw_fs/root/application/current/etc
upperdir=/rw_fs/root/upperdir/etc
workdir=/rw_fs/root/workdir/etc
mergedir=/etc

[PersistentMemory.config]
lowerdir=/rw_fs/root/application/current/config
upperdir=/rw_fs/root/upperdir/config
workdir=/rw_fs/root/workdir/config
mergedir=/config
```

| Field | Description |
|-------|-------------|
| `lowerdir` | Source directory (can be colon-separated for stacked layers) |
| `upperdir` | Writable upper layer directory on persistent storage |
| `workdir` | Working directory for overlayfs (must be on same filesystem as upperdir) |
| `mergedir` | Final mount point visible to the system |

### U-Boot Environment Variables

The tool reads the following U-Boot variables:

| Variable | Values | Description |
|----------|--------|-------------|
| `application` | A, B | Current active application slot |
| `BOOT_ORDER` | "A B", "B A" | Boot priority order |
| `BOOT_ORDER_OLD` | "A B", "B A" | Previous boot order (for rollback detection) |
| `BOOT_A_LEFT` | 0-3 | Remaining boot attempts for slot A |
| `BOOT_B_LEFT` | 0-3 | Remaining boot attempts for slot B |
| `update_reboot_state` | 0-12 | Current update state machine position |
| `rauc_cmd` | rauc.slot=A/B | RAUC slot selection |

## Boot Sequence

```
┌─────────────────────────────────────────────────────────────────┐
│                     dynamic_overlay                              │
└─────────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────────┐
│ 1. Mount /proc and /sys (PreInit stage 1)                       │
└─────────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────────┐
│ 2. Detect memory type (eMMC or NAND)                            │
│    - Read /sys/bdinfo/boot_dev or parse /proc/cmdline           │
└─────────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────────┐
│ 3. Mount persistent memory partition                            │
│    - eMMC: ext4 filesystem                                      │
│    - NAND: ubifs filesystem                                     │
└─────────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────────┐
│ 4. Create configuration symlinks                                │
│    - /etc/fw_env.config → memory-specific config                │
│    - /etc/rauc/system.conf → memory-specific config             │
└─────────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────────┐
│ 5. Read U-Boot environment and select application slot          │
│    - Check for failed update/rollback conditions                │
│    - Select app_a.squashfs or app_b.squashfs                    │
└─────────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────────┐
│ 6. Mount application image via loop device                      │
│    - Create loop device                                         │
│    - Mount squashfs to /rw_fs/root/application/current          │
└─────────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────────┐
│ 7. Parse overlay.ini from mounted application                   │
└─────────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────────┐
│ 8. Mount read-only overlays (ApplicationFolder)                 │
│    - Overlay application directories onto system                │
└─────────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────────┐
│ 9. Mount persistent overlays (PersistentMemory.*)               │
│    - Create upper/work directories                              │
│    - Copy permissions from system directories                   │
│    - Mount with read-write upper layer                          │
└─────────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────────┐
│ 10. (Optional) Extract and mount X.509 certificate store        │
└─────────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────────┐
│ 11. Unmount /proc and /sys                                      │
└─────────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────────┐
│ 12. Exit → init system takes over                               │
└─────────────────────────────────────────────────────────────────┘
```

## Directory Structure

```
/rw_fs/root/
├── application/
│   ├── current/           # Mount point for active squashfs
│   │   ├── overlay.ini    # Overlay configuration
│   │   ├── etc/           # Application's /etc overlay
│   │   ├── usr/           # Application's /usr overlay
│   │   └── ...
│   ├── app_a.squashfs     # Slot A application image
│   └── app_b.squashfs     # Slot B application image
├── upperdir/              # Persistent upper layers
│   ├── etc/
│   └── ...
├── workdir/               # Overlay work directories
│   ├── etc/
│   └── ...
└── conf/
    ├── fw_env.config      # U-Boot environment config
    └── system.conf        # RAUC system config
```

## Troubleshooting

### Common Issues

**"Could not determine current memory type"**
- Ensure `/proc` is mounted
- Check that `/sys/bdinfo/boot_dev` or `/proc/cmdline` contains valid boot device info

**"Application image not found"**
- Verify the squashfs file exists at `/rw_fs/root/application/app_[a|b].squashfs`
- Check U-Boot `application` variable

**"overlay.ini not found"**
- Ensure the application image is properly mounted
- Verify overlay.ini exists in the squashfs image

**"Maximum fs stacking depth exceeded"**
- Linux kernel limits overlay stacking to 2 levels
- Reduce the number of nested overlays in configuration

### Debug Output

Build with debug output enabled:

```bash
cmake .. -DCMAKE_CXX_FLAGS="-DDEBUG"
```

## License

Proprietary - F&S Elektronik Systeme GmbH

## See Also

- [F&S Update Framework Documentation](documentation/main_page.md)
- [RAUC Documentation](https://rauc.readthedocs.io/)
- [OverlayFS Documentation](https://docs.kernel.org/filesystems/overlayfs.html)
