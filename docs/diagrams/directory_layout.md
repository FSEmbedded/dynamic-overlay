# Directory Layout

## Persistent memory partition (`/rw_fs/root`)

Mounted with `MS_NOSUID | MS_NODEV` early in preinit.

```
/rw_fs/root/
├── application/
│   ├── current/              Mount point for active squashfs
│   │   ├── overlay.ini       Overlay configuration
│   │   ├── etc/              Application /etc content
│   │   ├── usr/              Application /usr content
│   │   └── ...
│   ├── app_a.squashfs        Slot A application image
│   └── app_b.squashfs        Slot B application image
├── upperdir/                 Persistent upper layers (read-write)
│   ├── etc/                  User modifications to /etc
│   └── ...
└── workdir/                  Overlay work directories
    ├── etc/
    └── ...
```

## Generated config (copies from memory-type-specific templates)

`create_link` picks the NAND or eMMC template for the detected boot
device and rewrites device paths before copying into the overlay-backed
`/etc` tree.

| File | Source template (in application image) | Purpose |
|------|------------------------------------------|---------|
| `/etc/fw_env.config` | `/etc/fw_env.config.{mmc,nand}` | `libubootenv` device binding |
| `/etc/rauc/system.conf` | `/etc/rauc/system.conf.{mmc,nand}` | RAUC slot/device binding |

## Mount Point Visibility

After preinit completes, the following are overlay mounts:

| Mount point | Type | Source |
|-------------|------|--------|
| `/etc` | Persistent overlay | app image + upperdir |
| `/usr/bin` | Read-only overlay | app image over system |
| (others per overlay.ini) | ... | ... |
| `/etc/adu` | Overlay-backed dir (0750, `adu:adu`) | Parent for cert tmpfs |
| `/etc/adu/certs` | tmpfs, `nosuid,nodev,noexec`, frozen R/O (optional) | Extracted from Secure partition, archive-authoritative |
