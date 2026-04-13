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
    ├── fw_env.config (U-Boot env access)
    └── system.conf (RAUC update manager)
    │
    ▼
 5. Read U-Boot variables, select A/B slot
    └── Check for failed update / rollback
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
 8. Mount read-only overlays (ApplicationFolder)
    │
    ▼
 9. Mount persistent overlays (PersistentMemory.*)
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
