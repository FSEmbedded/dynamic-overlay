# Directory Layout

Filesystem structure on the persistent memory partition.

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
├── workdir/                  Overlay work directories
│   ├── etc/
│   └── ...
└── conf/
    ├── fw_env.config         U-Boot environment config (generated)
    └── system.conf           RAUC system config (generated)
```

## Mount Point Visibility

After preinit completes, the following are overlay mounts:

| Mount point | Type | Source |
|-------------|------|--------|
| `/etc` | Persistent overlay | app image + upperdir |
| `/usr/bin` | Read-only overlay | app image over system |
| (others per overlay.ini) | ... | ... |
| `/etc/adu/certs` | tmpfs (optional) | Extracted from Secure partition |
