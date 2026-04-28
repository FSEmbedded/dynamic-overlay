# Boot Sequence

Preinit execution flow from kernel handoff to init system.

```
 1. Mount /proc and /sys
    │
    ▼
 2. Detect memory type (eMMC or NAND)
    ├── /sys/bdinfo/boot_dev (primary)
    └── /proc/cmdline root= (fallback)
    │
    ▼
 3. Mount persistent memory partition
    ├── eMMC → ext4
    └── NAND → ubifs
    │
    ▼
 4. Generate device config files
    ├── fw_env.config  (U-Boot environment access, symlinked from board-specific variant)
    └── system.conf    (RAUC configuration, symlinked from board-specific variant)
    │
    ▼
 5. Read U-Boot variables, select A/B slot
    └── Check for failed update / rollback via update_reboot_state
    │
    ▼
 6. Mount application squashfs via loop device
    └── app_a.squashfs or app_b.squashfs
    │
    ▼
 7. Parse overlay.ini from mounted application
    └── Fallback to compiled-in defaults if missing
    │
    ▼
 8. Mount read-only overlays (ApplicationFolder sections)
    │
    ▼
 9. Mount persistent overlays (PersistentMemory.* sections)
    └── Upgrades read-only mounts to read-write where both defined
    │
    ▼
10. (Optional) Extract X.509 certificates to tmpfs
    └── Failures never block boot
    │
    ▼
11. Unmount /proc and /sys
    │
    ▼
12. Exit → init system takes over
```

## Config file symlinks

At step 4, `dynamic_overlay` creates symlinks from the board-agnostic paths
to the storage-type-specific variants:

| Symlink | eMMC target | NAND target |
|---------|-------------|-------------|
| `/etc/fw_env.config` | `/etc/fw_env.config.mmc` | `/etc/fw_env.config.nand` |
| `/etc/rauc/system.conf` | `/etc/rauc/system.conf.mmc` | `/etc/rauc/system.conf.nand` |

Both `.mmc` and `.nand` variants must be present in the rootfs image.
See [Yocto Integration](yocto.md) for how to deploy them.

## Overlay stacking limit

Linux imposes a hard limit of 2 overlay levels on a single path. `dynamic_overlay`
enforces this at runtime: mounting a path that is already the target of an overlay
returns `Error::max_overlay_depth` (code 29) and aborts. Design your `overlay.ini`
so that no path requires more than one overlay layer above the base squashfs.
