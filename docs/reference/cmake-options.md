# CMake Options

All options are CMake cache variables. Override on the command line with
`-D<OPTION>=<value>` or via a Yocto recipe `EXTRA_OECMAKE`.

## Build behaviour

| Option | Type | Default | Description |
|--------|------|---------|-------------|
| `ENABLE_LOGGING` | bool | `ON` | Enable log output |
| `ENABLE_DEBUG_LOGGING` | bool | `OFF` | Enable debug-level log messages |
| `ENABLE_SANITIZERS` | bool | `OFF` | Enable ASan and UBSan (Debug builds only) |
| `BUILD_TESTING` | bool | `OFF` | Build unit test binary |
| `BUILD_MAIN_TARGET` | bool | `ON` | Build the main `dynamic_overlay` binary (set `OFF` when building tests only) |
| `BUILD_X509_CERTIFICATE_STORE_MOUNT` | bool | `OFF` | Build X.509 certificate store support |
| `LOG_BACKEND` | string | `STDERR` | Log sink: `KMSG` (production) or `STDERR` (development/tests) |
| `OPTIMIZE_FOR` | string | `SIZE` | Release optimisation: `SIZE` (`-Os`) or `SPEED` (`-O2`) |

## Filesystem paths

These are the compile-time default paths written into `src/config.h.in`.
All can be overridden at build time without source changes.

| Option | Default | Description |
|--------|---------|-------------|
| `PATH_TO_MOUNT_APPIMAGE` | `/rw_fs/root/application/current` | Mount point for the active application squashfs |
| `DEFAULT_OVERLAY_PATH` | `/rw_fs/root/application/current/overlay.ini` | Default `overlay.ini` path |
| `DEFAULT_APPLICATION_PATH` | `/rw_fs/root/application/current` | Application directory |
| `DEFAULT_UPPERDIR_PATH` | `/rw_fs/root/upperdir` | Overlay upper directory |
| `DEFAULT_WORKDIR_PATH` | `/rw_fs/root/workdir` | Overlay work directory |
| `APP_IMAGE_DIR` | `/rw_fs/root/application/` | Directory holding application slot images |
| `MAX_OVERLAY_COUNT` | `8` | Maximum number of overlay mounts (Linux caps stacking at 2 levels; this limits the count of separate overlay mountpoints) |
| `PERSISTENT_MEMORY_MOUNTPOINT` | `/rw_fs/root` | Persistent memory mount point |
| `PERSISTMEMORY_DEVICE_NAME` | `data` | Persistent memory block device label (looked up via libblkid) |

## U-Boot and RAUC paths

| Option | Default | Description |
|--------|---------|-------------|
| `UBOOT_ENV_PATH` | `/etc/fw_env.config` | Symlink target for the active U-Boot env config |
| `EMMC_UBOOT_ENV_PATH` | `/etc/fw_env.config.mmc` | U-Boot env config for eMMC boards |
| `NAND_UBOOT_ENV_PATH` | `/etc/fw_env.config.nand` | U-Boot env config for NAND boards |
| `RAUC_SYSTEM_CONF_PATH` | `/etc/rauc/system.conf` | Symlink target for the active RAUC system.conf |
| `EMMC_RAUC_SYSTEM_CONF_PATH` | `/etc/rauc/system.conf.mmc` | RAUC system.conf for eMMC boards |
| `NAND_RAUC_SYSTEM_CONF_PATH` | `/etc/rauc/system.conf.nand` | RAUC system.conf for NAND boards |

## X.509 certificate store (optional)

These options are only relevant when `BUILD_X509_CERTIFICATE_STORE_MOUNT=ON`.

| Option | Default | Description |
|--------|---------|-------------|
| `TARGET_ARCHIV_DIR_PATH` | _(empty)_ | tmpfs mount point for extracted certificate archive |
| `FUS_AZURE_CONFIGURATION` | _(empty)_ | Azure DU config path |
| `PART_NAME_MTD_CERT` | _(empty)_ | MTD partition name for the certificate store (NAND) |
| `EMMC_SECURE_PART_BLK_NR` | _(empty)_ | eMMC secure partition block number |
