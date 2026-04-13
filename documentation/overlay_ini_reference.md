# overlay.ini Configuration Reference

## Overview

The `overlay.ini` file defines the overlay filesystem configuration for the F&S Update Framework. It is located inside the application squashfs image at:

```
/rw_fs/root/application/current/overlay.ini
```

This file is read after the application image is mounted and controls which directories are overlaid onto the root filesystem.

## File Format

The configuration uses standard INI format with two types of sections:
- `[ApplicationFolder]` - Read-only overlay entries
- `[PersistentMemory.*]` - Read-write persistent overlay sections

## Section Types

### ApplicationFolder Section

The `[ApplicationFolder]` section defines directories from the application image that should be overlaid onto the root filesystem as **read-only** overlays.

#### Syntax

```ini
[ApplicationFolder]
entry1=<path>
entry2=<path>
...
```

#### Fields

| Field | Type | Description |
|-------|------|-------------|
| `entryN` | path | Absolute path to overlay (e.g., `/etc`, `/usr/bin`) |

#### Behavior

For each entry:
1. The tool creates a read-only overlay
2. Lower directories are combined:
   - `/rw_fs/root/application/current/<path>` (application content, highest priority)
   - `<path>` (original system content)
3. The overlay is mounted at `<path>`

#### Example

```ini
[ApplicationFolder]
entry1=/etc
entry2=/usr/bin
entry3=/opt/application
entry4=/var/lib/app
```

This configuration will:
- Overlay `/rw_fs/root/application/current/etc` onto `/etc`
- Overlay `/rw_fs/root/application/current/usr/bin` onto `/usr/bin`
- Overlay `/rw_fs/root/application/current/opt/application` onto `/opt/application`
- Overlay `/rw_fs/root/application/current/var/lib/app` onto `/var/lib/app`

#### Notes

- Entry names must follow the pattern `entry<N>` where N is a number
- Paths must be absolute (start with `/`)
- The application directory content takes precedence over system content
- No writes are possible (read-only)
- Maximum of 8 overlays can be mounted (kernel limitation)

---

### PersistentMemory Sections

`[PersistentMemory.*]` sections define directories that need **read-write** persistent storage. These overlays allow modifications to persist across reboots.

#### Syntax

```ini
[PersistentMemory.<name>]
lowerdir=<path>[:<path>...]
upperdir=<path>
workdir=<path>
mergedir=<path>
```

#### Fields

| Field | Required | Type | Description |
|-------|----------|------|-------------|
| `lowerdir` | Yes | path(s) | Lower layer directory or colon-separated list |
| `upperdir` | Yes | path | Upper layer directory (must be on writable filesystem) |
| `workdir` | Yes | path | Work directory (must be on same filesystem as upperdir) |
| `mergedir` | Yes | path | Final mount point (visible to system) |

#### Field Details

**lowerdir**
- Source directory containing base content
- Can be a single path or multiple colon-separated paths
- Rightmost path has lowest priority, leftmost has highest
- Example: `lowerdir=/app/current/etc:/etc` means `/app/current/etc` overlays `/etc`

**upperdir**
- Directory where modifications are stored
- Must be on a writable filesystem (typically ext4 on the data partition)
- Created automatically if `create_dirs` would be supported
- Permissions are copied from the system directory before mounting

**workdir**
- Internal working directory for overlayfs
- **Must be on the same filesystem as upperdir**
- Should be empty before mount
- Created automatically alongside upperdir

**mergedir**
- The final mount point where the overlay becomes visible
- This is the path applications will use to access the overlaid directory
- Typically matches one of the lowerdir paths

#### Example

```ini
[PersistentMemory.etc]
lowerdir=/rw_fs/root/application/current/etc
upperdir=/rw_fs/root/upperdir/etc
workdir=/rw_fs/root/workdir/etc
mergedir=/etc

[PersistentMemory.config]
lowerdir=/rw_fs/root/application/current/config:/config
upperdir=/rw_fs/root/upperdir/config
workdir=/rw_fs/root/workdir/config
mergedir=/config

[PersistentMemory.var_lib]
lowerdir=/var/lib
upperdir=/rw_fs/root/upperdir/var_lib
workdir=/rw_fs/root/workdir/var_lib
mergedir=/var/lib
```

#### Notes

- Section name must match pattern `PersistentMemory.<identifier>`
- The identifier can be any string (e.g., `etc`, `config`, `myapp`)
- All four fields are required
- upperdir and workdir must be on the same mounted filesystem
- Changes written to mergedir are stored in upperdir
- Original lower layer content is never modified

---

## Complete Example

```ini
; Read-only application overlays
[ApplicationFolder]
entry1=/etc
entry2=/usr/bin
entry3=/opt/myapp
entry4=/usr/share/myapp

; Persistent overlay for /etc (allows config modifications)
[PersistentMemory.etc]
lowerdir=/rw_fs/root/application/current/etc
upperdir=/rw_fs/root/upperdir/etc
workdir=/rw_fs/root/workdir/etc
mergedir=/etc

; Persistent overlay for application data
[PersistentMemory.appdata]
lowerdir=/rw_fs/root/application/current/var/lib/myapp
upperdir=/rw_fs/root/upperdir/var_lib_myapp
workdir=/rw_fs/root/workdir/var_lib_myapp
mergedir=/var/lib/myapp

; Persistent overlay for logs
[PersistentMemory.logs]
lowerdir=/var/log
upperdir=/rw_fs/root/upperdir/var_log
workdir=/rw_fs/root/workdir/var_log
mergedir=/var/log
```

## Processing Order

1. **ApplicationFolder entries** are processed first
   - Creates read-only overlays for application content

2. **PersistentMemory sections** are processed second
   - If a path was already overlaid by ApplicationFolder, the read-only overlay is **unmounted** and replaced with the persistent overlay
   - This allows PersistentMemory to "upgrade" a read-only overlay to read-write

## Interaction Between Sections

When the same path appears in both sections:

```ini
[ApplicationFolder]
entry1=/etc           ; First: mounts read-only overlay

[PersistentMemory.etc]
lowerdir=/rw_fs/root/application/current/etc
upperdir=/rw_fs/root/upperdir/etc
workdir=/rw_fs/root/workdir/etc
mergedir=/etc         ; Second: unmounts read-only, mounts read-write
```

Result: `/etc` ends up as a **persistent read-write overlay** with the application content as the lower layer.

## Error Handling

| Error Condition | Behavior |
|-----------------|----------|
| overlay.ini not found | Uses fallback: overlay `/etc` and `/usr/bin` only |
| Invalid section name | Error logged, section skipped |
| Missing required field | ConfigException thrown |
| Directory doesn't exist | Warning logged, overlay skipped |
| Mount fails (EBUSY) | Already mounted, continue |
| Max stacking depth | Warning logged, remaining overlays skipped |

## Best Practices

1. **Keep overlays minimal**: Only overlay directories that need application-specific content

2. **Use PersistentMemory for config**: If users need to modify files (e.g., `/etc/network`), use PersistentMemory instead of ApplicationFolder

3. **Avoid deep nesting**: Overlaying `/etc` and `/etc/subdir` separately can cause issues

4. **Consider disk space**: upperdir grows as files are modified - ensure sufficient space on data partition

5. **Test rollback**: Verify that A/B updates work correctly with your overlay configuration

## Limitations

- Maximum 8 overlay mounts (configurable via `Config::MAX_OVERLAY_COUNT`)
- Linux kernel limits overlay stacking to 2 levels
- upperdir and workdir must be on same filesystem
- Cannot overlay root (`/`) directly
- File permissions may differ between application squashfs and system

## Troubleshooting

**Overlay not visible after boot**
- Check if the path exists in the application image
- Verify overlay.ini syntax
- Check dmesg for mount errors

**Permission denied errors**
- The tool automatically copies permissions from system to upper directory
- Check if the source directory has correct permissions

**Files disappear after update**
- PersistentMemory upperdir preserves changes across updates
- ApplicationFolder content comes from the new application image

**"Maximum fs stacking depth exceeded"**
- Reduce number of nested overlays
- Avoid overlaying directories that are already overlays
