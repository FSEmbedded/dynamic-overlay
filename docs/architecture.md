# Dynamic Overlay Architecture

## Overview

Preinit-stage tool that prepares the overlay filesystem structure for A/B
updates. Executes before any init system, so errors here prevent boot.

For the visual overview see
[Component Architecture](diagrams/component_architecture.md) and
[Boot Sequence](diagrams/boot_sequence.md).

## Modules

### PreInit (preinit.h/cpp)

Ordered mount/unmount for early boot virtual filesystems (`/proc`, `/sys`,
persistent memory). On failure, rollback unmounts in reverse order to avoid
EBUSY on stacked mounts.

```cpp
struct MountArgs {
    std::string source_dir;
    std::string dest_dir;
    std::string options;
    std::string filesystem_type;
    unsigned long flags;
};
```

### PersistentMemDetector (persistent_mem_detector.h/cpp)

Detects boot storage type and locates the data partition.

**Detection order:**
1. `/sys/bdinfo/boot_dev` (vendor sysfs)
2. `/proc/cmdline` root= parameter (fallback)

**Partition discovery:**
- eMMC: `blkid_find_dev_with_tag(LABEL, "data")`
- NAND: scan `/sys/class/ubi/ubiX/ubiX_Y/name` for volume "data"

### UBoot (u-boot.h/cpp)

Type-safe U-Boot variable access via libubootenv. Each read opens a fresh
context to avoid stale data. Typed overloads validate against allowed value
sets before returning.

### DynamicMounting (dynamic_mounting.h/cpp)

Central orchestrator. Parses `overlay.ini`, selects A/B slot, mounts overlays.

**A/B slot selection:**
- Normal: use slot from `application` variable (A or B). See
  `DynamicMounting::determine_application_image` in
  `src/dynamic_mounting.cpp`.
- Failed update: `detect_failedUpdate_app_fw_reboot` combines
  `BOOT_ORDER` vs `BOOT_ORDER_OLD`, `rauc_cmd`, and remaining tries
  (`BOOT_A_LEFT` / `BOOT_B_LEFT`). The fallback branch consults
  `update_reboot_state` (rollback states `ROLLBACK_APP_FW_REBOOT_PENDING`
  and `INCOMPLETE_APP_FW_ROLLBACK` — constants in `dynamic_mounting.h`).

**Overlay processing order:**
1. Mount application squashfs via loop device
2. Parse `overlay.ini` (or use compiled-in fallback)
3. Mount ApplicationFolder entries (read-only)
4. Mount PersistentMemory entries (read-write, may upgrade read-only mounts)

See [Mount Sequence](diagrams/mount_sequence.md) for the flow diagram.

### overlay_config (overlay_config.h/cpp)

Pure parser: turns an `ini::Section` for a `PersistentMemory.*` block into
an `OverlayDescription::Persistent`. Extracted from `DynamicMounting` so
the validation logic is unit-testable without pulling in UBoot/libblkid.

Enforces:
- Required keys `lowerdir`, `upperdir`, `workdir`, `mergedir`.
- Optional key `nosuid` ∈ {`true`, `false`} (case-sensitive). Any other
  value → `Error::config_invalid`. Default is `true`.
- Any unknown key → `Error::config_invalid`.

### Mount (mount.h/cpp)

Low-level mount operations using Linux syscalls directly.

| Operation | Method |
|-----------|--------|
| Squashfs app image | Loop device via `/dev/loop-control` + `mount(squashfs)` |
| Read-only overlay | `mount(overlay, MS_RDONLY)` with `lowerdir=...,xino=auto` |
| Persistent overlay | `mount(overlay)` with `upperdir=...,workdir=...,lowerdir=...,index=on` |

Mount-flag hardening: `nosuid=false` opt-out schema and parser-rejection
behavior are in [`overlay.ini Reference`](reference/overlay-ini.md).
Implementation is split across `src/main.cpp` (data partition),
`src/mount.cpp` (overlay mounts), and `src/overlay_config.cpp`
(per-section parsing).

### file_properties (file_properties.h/cpp)

Syncs uid, gid, mode, and xattrs from lower to upper directory before
persistent overlay mount. Without this, overlayfs presents inconsistent
ownership.

### create_link (create_link.h/cpp)

Generates `fw_env.config` and `system.conf` from memory-type-specific
templates, rewriting device paths to match the actual boot device.
Uses atomic write-to-temp then rename.

### x509_cert_store (x509_cert_store.h/cpp) — optional

Extracts an archive of X.509 material from the Secure partition (NAND MTD
or eMMC block offset) to a volatile tmpfs staging area. Built with
`--x509` flag.

The provisioning archive is the **single source of truth**. On boot the
staged `du-config.json` is validated (`connectionType` must be `x509`,
`device_id` and `iotHubName` non-empty, `x509_container` pinned to the
compile-time path, cert/key basenames on an allow-list) and atomically
published to the overlay via write-to-tmp + rename + `fsync`.

libarchive extraction runs with `secure-nodotdot`, `secure-symlinks` and
`no-overwrite`; only regular files on the allow-list (`du-config.json`,
`*.cert.pem`, `*.key.pem`) are written; per-entry and total-archive size
caps bound the tmpfs footprint.

The cert tmpfs is mounted `nosuid,nodev,noexec` with `adu:adu` ownership
and mode `0750` from the start; the post-publish freeze remount preserves
those flags. The overlay-backed `/etc/adu` parent is `chown`ed and
`chmod 0750` after `mkdir_p` so the ADU agent's
`CheckConfDirOwnershipAndPermissions` passes.

Cert store failures never block boot. Temp files cleaned via `ScopeGuard`.

### Shared foundation

Small TUs used throughout the codebase:

| Module | Purpose |
|--------|---------|
| `error.h` | Project-wide `enum class Error : uint8_t` (ABI-stable, 34 codes) |
| `logging.h` | Compile-out-able `LOG_*` macros; writes `/dev/kmsg` in preinit, `stderr` in test builds |
| `posix_utils.*` | `FdGuard`, `DirGuard`, `ScopeGuard`, path predicates, recursive chown |
| `string_utils.h` | `trim`, `split`, `join`, `to_lower` — header-only, exception-free |
| `ini_parser.*` | Minimal INI parser; ordered sections preserved, unit-tested |
| `device_parser.*` | Parses `/proc/cmdline root=` into device/partition (fallback for memory-type detection) |

## Error Handling

No exceptions (`-fno-exceptions`). All functions return
`[[nodiscard]] enum class Error` with `noexcept`. Error codes are ABI-stable
with explicit values (see `error.h`).

### Recovery strategy

| Stage | On failure |
|-------|------------|
| PreInit mounts | Rollback all in reverse order |
| Application mount | Error propagated, overlay setup continues |
| Individual overlay | Warning logged, remaining overlays still mount |
| Cert store | tmpfs unmounted via ScopeGuard, boot continues |
| Cert store `du-config.json` invalid | No publish, overlay keeps prior state (or empty) — ADU agent stays unprovisioned rather than booting with garbage identity |
| overlay.ini missing | Compiled-in fallback config (`/etc`, `/usr/bin`) |

## Resource Management

All system resources use RAII:

| Resource | Guard |
|----------|-------|
| File descriptors | `FdGuard` (loop devices, raw partitions, config files) |
| Scope cleanup | `ScopeGuard` (tmpfs unmount, temp file removal) |
| `DIR*` handles | `DirGuard` (UBI volume scan, recursive chown) |
| `blkid_cache` | `BlkidCacheGuard` (eMMC partition discovery in `persistent_mem_detector`) |
| libuboot context | `UBootCtxGuard` (init/open/close/exit lifecycle) |
| libarchive handles | `unique_ptr` with custom deleters |

No manual memory management, no raw `new`/`delete`.

## Build-time Configuration

All paths are CMake cache variables, configurable via `-D` flags or Yocto
recipe. See `config.h.in` for the generated header.

| Variable | Default | Description |
|----------|---------|-------------|
| `PATH_TO_MOUNT_APPIMAGE` | `/rw_fs/root/application/current` | Application mount point |
| `DEFAULT_OVERLAY_PATH` | `/rw_fs/root/application/current/overlay.ini` | Config file path |
| `DEFAULT_UPPERDIR_PATH` | `/rw_fs/root/upperdir` | Default upper directory base |
| `DEFAULT_WORKDIR_PATH` | `/rw_fs/root/workdir` | Default work directory base |
| `MAX_OVERLAY_COUNT` | `8` | Maximum overlay mounts |
| `PERSISTMEMORY_DEVICE_NAME` | `data` | Persistent partition label |
| `PERSISTENT_MEMORY_MOUNTPOINT` | `/rw_fs/root` | Persistent mount point |

## Thread Safety

Single-threaded by design. Runs during preinit before any services start.
Mount operations are inherently sequential.
