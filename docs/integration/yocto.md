# Yocto Integration

## Partition layout

`dynamic_overlay` requires two types of storage partitions:

| Partition | Filesystem | Mount point | Purpose |
|-----------|-----------|-------------|---------|
| Root A / Root B | squashfs (read-only) | via loop device | Firmware slots |
| Application A / Application B | squashfs (read-only) | `/rw_fs/root/application/current` | Application slots |
| Data | ext4 (eMMC) / ubifs (NAND) | `/rw_fs/root` | Persistent overlay upperdirs and workdirs |

The data partition is mounted with `MS_NOSUID | MS_NODEV` to prevent privilege
escalation through user-writable content.

## NAND: mtdparts and use_ab

`mtdparts` defines the NAND partition layout and **must** match the `use_ab`
U-Boot variable:

| `use_ab` | Expected partition names |
|----------|------------------------|
| `true` | `Kernel_A`, `FDT_A`, `Kernel_B`, `FDT_B`, `Secure` |
| `false` | `Kernel`, `FDT` (legacy layout) |

The U-Boot boot script runs `.mtdparts_std` on every boot to re-evaluate the
partition table. This is required because `setup_var()` in `board_late_init()`
only initialises `mtdparts` on the first boot (when the value is `"undef"`).
Without re-evaluation, changing `use_ab` after the first boot leaves the old
partition table in place and causes `nand read` to fail on missing partition
names.

> **If you add partition-dependent boot logic**, ensure it runs **after**
> `run .mtdparts_std` inside `select_boot_mode`.

## fw_env.config

`libubootenv` requires `/etc/fw_env.config` to locate the U-Boot environment
partition. `dynamic_overlay` generates this symlink at boot time (step 4 of
the boot sequence) pointing to the board-specific variant.

Provide two files in your rootfs image:

```
/etc/fw_env.config.mmc   ← for eMMC boards
/etc/fw_env.config.nand  ← for NAND boards
```

Typical `fw_env.config.mmc`:

```
# device       offset          size      sector_size  nrofcopies
/dev/mmcblk2boot0  0x0000      0x20000   0x200
/dev/mmcblk2boot0  0x20000     0x20000   0x200
```

Typical `fw_env.config.nand`:

```
# device    offset      size
/dev/mtd1   0x0         0x20000
/dev/mtd2   0x0         0x20000
```

Adjust device paths and offsets to match your board's memory map.

## RAUC system.conf

`dynamic_overlay` generates the `/etc/rauc/system.conf` symlink at the same
time as `fw_env.config`. Provide:

```
/etc/rauc/system.conf.mmc
/etc/rauc/system.conf.nand
```

See [fs-updater-lib RAUC integration](https://github.com/fsembedded/fs-updater-lib/blob/main/docs/integration/rauc-system-conf.md)
for the full `system.conf` layout.

## Yocto recipe integration

In your BSP layer, add a `.bbappend` for `dynamic-overlay`:

```bitbake
FILESEXTRAPATHS:prepend := "${THISDIR}/files:"

SRC_URI += " \
    file://fw_env.config.mmc \
    file://fw_env.config.nand \
    file://system.conf.mmc \
    file://system.conf.nand \
"

do_install:append() {
    install -d ${D}${sysconfdir}
    install -m 0644 ${WORKDIR}/fw_env.config.mmc  ${D}${sysconfdir}/fw_env.config.mmc
    install -m 0644 ${WORKDIR}/fw_env.config.nand ${D}${sysconfdir}/fw_env.config.nand
    install -d ${D}${sysconfdir}/rauc
    install -m 0644 ${WORKDIR}/system.conf.mmc    ${D}${sysconfdir}/rauc/system.conf.mmc
    install -m 0644 ${WORKDIR}/system.conf.nand   ${D}${sysconfdir}/rauc/system.conf.nand
}
```

## CMake build options for Yocto

Set these via `EXTRA_OECMAKE` in your `.bbappend`:

```bitbake
EXTRA_OECMAKE += " \
    -DLOG_BACKEND=KMSG \
    -DPERSISTMEMORY_DEVICE_NAME=data \
    -DUBOOT_ENV_NAND=mtd1 \
"
```

Common overrides:

| Option | What to adjust |
|--------|---------------|
| `UBOOT_ENV_PATH` | Path to the symlink `dynamic_overlay` creates |
| `EMMC_UBOOT_ENV_PATH` / `NAND_UBOOT_ENV_PATH` | Paths to the board-specific `fw_env.config` files |
| `PERSISTMEMORY_DEVICE_NAME` | Label of the data partition (from `blkid`) |
| `PERSISTENT_MEMORY_MOUNTPOINT` | Mount point for the data partition |
| `MAX_OVERLAY_COUNT` | Increase if your `overlay.ini` defines more than 8 mountpoints |

See [CMake Options](../reference/cmake-options.md) for the full list.
