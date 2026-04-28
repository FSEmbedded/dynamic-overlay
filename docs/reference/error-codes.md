# Error Codes

`dynamic_overlay` exits with a value from the `Error` enum defined in
`src/error.h`. Values are ABI-stable and append-only — do not renumber or
reuse existing codes.

The exit code is the numeric value of the enum member cast to `uint8_t`.

| Code | Name | Description |
|-----:|------|-------------|
| 0 | `none` | Success |
| 1 | `mount_failed` | `mount(2)` syscall failed |
| 2 | `umount_failed` | `umount(2)` syscall failed |
| 3 | `loop_device_failed` | Loop device setup or teardown failed |
| 4 | `overlay_mount_failed` | Overlay filesystem mount failed |
| 5 | `directory_create_failed` | `mkdir(2)` failed |
| 6 | `config_invalid` | `overlay.ini` content failed validation |
| 7 | `config_not_found` | `overlay.ini` file not found at expected path |
| 8 | `config_parse_failed` | `overlay.ini` could not be parsed (syntax error) |
| 9 | `uboot_init_failed` | libubootenv initialisation failed |
| 10 | `uboot_read_failed` | U-Boot environment read failed |
| 11 | `uboot_var_not_found` | Required U-Boot variable missing |
| 12 | `uboot_var_invalid` | U-Boot variable holds an unexpected value |
| 13 | `permission_denied` | `EACCES` / `EPERM` on a filesystem operation |
| 14 | `stat_failed` | `stat(2)` failed |
| 15 | `chmod_failed` | `chmod(2)` failed |
| 16 | `chown_failed` | `chown(2)` failed |
| 17 | `open_failed` | `open(2)` failed |
| 18 | `read_failed` | `read(2)` failed |
| 19 | `write_failed` | `write(2)` failed |
| 20 | `close_failed` | `close(2)` failed |
| 21 | `path_not_found` | A required path does not exist |
| 22 | `memory_detect_failed` | Boot storage type detection failed |
| 23 | `blkid_failed` | libblkid lookup failed |
| 24 | `symlink_failed` | `symlink(2)` failed |
| 25 | `copy_failed` | File copy failed |
| 26 | `rename_failed` | `rename(2)` failed |
| 27 | `already_mounted` | Attempted to mount a path already mounted |
| 28 | `not_mounted` | Attempted to unmount a path not mounted |
| 29 | `max_overlay_depth` | Maximum overlay stacking depth exceeded (Linux limit: 2 levels) |
| 30 | `invalid_argument` | Internal logic received an out-of-range argument |
| 31 | `cert_store_failed` | X.509 certificate store operation failed (only with `--x509`) |
| 32 | `json_parse_failed` | JSON parse failed (cert store config) |
| 33 | `remove_failed` | `remove(2)` / `unlink(2)` failed |

## Extension rules

New error codes must be appended after the current last value (`33`). Do not
insert values between existing codes. Update this table and `src/error.h`
in the same commit.
