# Mount Sequence

How overlay.ini is parsed and overlays are mounted.

## Configuration Parsing

```
overlay.ini
    │
    ├── [ApplicationFolder]
    │   ├── entry1=/etc         ──► overlay_application_ vector
    │   ├── entry2=/usr/bin
    │   └── ...
    │
    └── [PersistentMemory.etc]
        ├── lowerdir=...        ──► overlay_persistent_ map
        ├── upperdir=...               (key = section name)
        ├── workdir=...
        └── mergedir=...
```

## Overlay Mount Flow

```
                    ┌──────────────────┐
                    │   overlay.ini    │
                    └────────┬─────────┘
                             │
                             ▼
              ┌──────────────────────────────┐
              │     Parse Configuration      │
              └──────────────────────────────┘
                             │
         ┌───────────────────┴───────────────────┐
         │                                       │
         ▼                                       ▼
┌─────────────────────┐               ┌─────────────────────┐
│  ApplicationFolder  │               │  PersistentMemory   │
│      entries        │               │      sections       │
└─────────┬───────────┘               └──────────┬──────────┘
          │                                      │
          ▼                                      ▼
┌─────────────────────┐               ┌─────────────────────┐
│ For each entry:     │               │ For each section:   │
│                     │               │                     │
│ lowerdir =          │               │ Create upperdir     │
│   /app/current/path │               │ Create workdir      │
│   : /path           │               │ Copy permissions    │
│                     │               │                     │
│ Mount R/O overlay   │               │ Mount R/W overlay   │
│ at /path            │               │ at mergedir         │
└─────────────────────┘               └─────────────────────┘
```

## Upgrade: Read-Only to Read-Write

When the same path appears in both ApplicationFolder and PersistentMemory,
the read-only overlay is unmounted and replaced with a persistent overlay
that includes the application content as a lower layer:

```
Step 1: ApplicationFolder mounts /etc as read-only
        lowerdir=/app/current/etc:/etc

Step 2: PersistentMemory.etc unmounts /etc, remounts as read-write
        lowerdir=/app/current/etc:/etc
        upperdir=/rw_fs/root/upperdir/etc
        workdir=/rw_fs/root/workdir/etc
```
