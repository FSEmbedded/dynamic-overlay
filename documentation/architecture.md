# Dynamic Overlay Architecture

## Overview

Dynamic Overlay is designed as a preinit-stage tool that executes before the main init system. Its primary purpose is to prepare the overlay filesystem structure that allows the F&S Update Framework to function with A/B updates.

## Component Architecture

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                              main.cpp                                        │
│                           (Entry Point)                                      │
└─────────────────────────────────────────────────────────────────────────────┘
                                    │
         ┌──────────────────────────┼──────────────────────────┐
         │                          │                          │
         ▼                          ▼                          ▼
┌─────────────────┐      ┌─────────────────────┐      ┌─────────────────┐
│    PreInit      │      │  PersistentMem      │      │     UBoot       │
│                 │      │     Detector        │      │                 │
│  - Mount /proc  │      │                     │      │  - Read vars    │
│  - Mount /sys   │      │  - Detect eMMC/NAND │      │  - Validate     │
│  - Mount data   │      │  - Find partitions  │      │  - Type conv    │
└─────────────────┘      └─────────────────────┘      └─────────────────┘
         │                          │                          │
         └──────────────────────────┼──────────────────────────┘
                                    │
                                    ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│                          DynamicMounting                                     │
│                                                                              │
│  - Parse overlay.ini                                                         │
│  - Manage overlay lists (ApplicationFolder, PersistentMemory)               │
│  - Orchestrate mount sequence                                                │
│  - Handle A/B slot selection                                                 │
│  - Detect failed updates                                                     │
└─────────────────────────────────────────────────────────────────────────────┘
                                    │
                                    ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│                              Mount                                           │
│                                                                              │
│  - mount_application_image()    : Loop device + squashfs mount              │
│  - mount_overlay_persistent()   : R/W overlay with upperdir                 │
│  - mount_overlay_readonly()     : R/O overlay (multiple lowerdirs)          │
│  - wrapper_c_mount()            : Generic mount syscall wrapper             │
│  - wrapper_c_umount()           : Generic umount syscall wrapper            │
└─────────────────────────────────────────────────────────────────────────────┘
                                    │
              ┌─────────────────────┼─────────────────────┐
              │                     │                     │
              ▼                     ▼                     ▼
┌─────────────────────┐  ┌─────────────────────┐  ┌─────────────────────┐
│   file_properties   │  │    create_link      │  │   x509_cert_store   │
│                     │  │                     │  │     (optional)      │
│ - Copy permissions  │  │ - Link fw_env.conf  │  │                     │
│ - Copy xattrs       │  │ - Link system.conf  │  │ - Extract certs     │
│ - Validate props    │  │ - Update device     │  │ - Mount cert store  │
└─────────────────────┘  └─────────────────────┘  └─────────────────────┘
```

## Class Descriptions

### PreInit (preinit.h/cpp)

**Purpose**: Manages early-stage mount operations with automatic cleanup on failure.

**Key Features**:
- Queues mount operations before execution
- Automatic rollback: if any mount fails, previously mounted filesystems are unmounted
- Used for mounting /proc, /sys, and persistent storage

**Data Structures**:
```cpp
struct MountArgs {
    std::string source_dir;      // Source device or directory
    std::string dest_dir;        // Mount point
    std::string options;         // Mount options
    std::string filesystem_type; // e.g., "proc", "sysfs", "ext4"
    unsigned long flags;         // Mount flags (MS_NOSUID, etc.)
};
```

### PersistentMemDetector (persistent_mem_detector.h/cpp)

**Purpose**: Detects the type of persistent storage and locates the data partition.

**Detection Methods**:
1. Primary: Read `/sys/bdinfo/boot_dev` (F&S-specific sysfs entry)
2. Fallback: Parse `/proc/cmdline` for root device

**Memory Type Mapping**:
| boot_dev value | Memory Type | Filesystem |
|----------------|-------------|------------|
| mmc1 | eMMC (mmcblk0) | ext4 |
| mmc2 | eMMC (mmcblk1) | ext4 |
| mmc3 | eMMC (mmcblk2) | ext4 |
| *nand* | NAND | ubifs |

**Partition/Volume Discovery**:
- eMMC: Uses libblkid to find partition by LABEL="data"
- NAND: Scans /sys/class/ubi/ubiX/ubiX_Y/name for volume named "data"

### UBoot (u-boot.h/cpp)

**Purpose**: Provides type-safe access to U-Boot environment variables.

**API Design**:
```cpp
// Type-specific getters with validation
uint8_t getVariable(name, allowed_list<uint8_t>);  // Numeric variables
std::string getVariable(name, allowed_list<string>); // String variables
char getVariable(name, allowed_list<char>);         // Single char variables
```

**Validation**: All getters validate that the returned value is in an allowed list, preventing unexpected values from causing undefined behavior.

### DynamicMounting (dynamic_mounting.h/cpp)

**Purpose**: Central orchestrator for the overlay mount process.

**Key Responsibilities**:
1. Parse overlay.ini configuration
2. Determine which application image to mount (A/B selection)
3. Detect failed update conditions
4. Mount overlays in correct order

**A/B Slot Selection Logic**:
```
IF no failed update detected:
    Use slot indicated by 'application' variable (A or B)
ELSE (failed update):
    IF application=B AND (rollback_pending OR incomplete_rollback):
        Use slot B
    ELSE IF application=A AND NOT (rollback_pending OR incomplete_rollback):
        Use slot B (fallback to previous working)
    ELSE:
        Use slot A (default)
```

**Overlay Processing Order**:
1. Mount application squashfs
2. Parse overlay.ini
3. Mount ApplicationFolder entries (read-only)
4. Mount PersistentMemory entries (read-write)
5. Mount additional ReadOnly entries (e.g., certificate store)

### Mount (mount.h/cpp)

**Purpose**: Low-level mount operations using Linux syscalls.

**Squashfs Mount Process**:
```
1. Open /dev/loop-control
2. Get free loop device number (LOOP_CTL_GET_FREE)
3. Open /dev/loopN
4. Open squashfs image file
5. Associate loop device with file (LOOP_SET_FD)
6. Mount loop device as squashfs
```

**Overlay Mount Options**:
| Mount Type | Options |
|------------|---------|
| Read-only | `lowerdir=path1:path2:...,xino=auto` |
| Persistent | `upperdir=...,workdir=...,lowerdir=...,index=on,xino=auto` |

### OverlayDescription (mount.h)

**Purpose**: Data classes for overlay configuration.

```cpp
// For read-only overlays (no upper layer)
class ReadOnly {
    std::string lower_directory;  // Colon-separated lower dirs
    std::string merge_directory;  // Final mount point
};

// For persistent overlays (with upper layer)
class Persistent {
    std::string lower_directory;  // Colon-separated lower dirs
    std::string upper_directory;  // Writable upper layer
    std::string work_directory;   // Overlay work directory
    std::string merge_directory;  // Final mount point
};
```

### file_properties (file_properties.h/cpp)

**Purpose**: Ensures correct permissions on overlay directories.

**Problem Solved**: When creating persistent overlays, the upper directory might have different permissions than the system directory it overlays. This can cause permission issues.

**Solution**: Copy permissions, ownership, and extended attributes from the system (lower) directory to the upper directory before mounting.

### create_link (create_link.h/cpp)

**Purpose**: Creates and updates configuration files for the detected boot device.

**Configuration Files Updated**:
- `/rw_fs/root/conf/fw_env.config` - U-Boot environment access configuration
- `/rw_fs/root/conf/system.conf` - RAUC update configuration

**Device Path Updates**: Replaces generic device paths (e.g., `/dev/mmcblk0`) with the actual detected boot device.

## Data Flow

### Configuration Parsing

```
overlay.ini
    │
    ├── [ApplicationFolder]
    │   ├── entry1=/etc        ──────► overlay_application vector
    │   ├── entry2=/usr/bin
    │   └── ...
    │
    └── [PersistentMemory.etc]
        ├── lowerdir=...       ──────► overlay_persistent map
        ├── upperdir=...              (key = section name)
        ├── workdir=...
        └── mergedir=...
```

### Mount Sequence

```
                        ┌─────────────────┐
                        │ overlay.ini     │
                        └────────┬────────┘
                                 │
                                 ▼
              ┌──────────────────────────────────────┐
              │        Parse Configuration           │
              └──────────────────────────────────────┘
                                 │
         ┌───────────────────────┴───────────────────────┐
         │                                               │
         ▼                                               ▼
┌─────────────────────┐                      ┌─────────────────────┐
│  ApplicationFolder  │                      │  PersistentMemory   │
│      entries        │                      │      sections       │
└─────────┬───────────┘                      └──────────┬──────────┘
          │                                             │
          ▼                                             ▼
┌─────────────────────┐                      ┌─────────────────────┐
│ For each entry:     │                      │ For each section:   │
│                     │                      │                     │
│ lowerdir =          │                      │ Create upperdir     │
│   /app/current/path │                      │ Create workdir      │
│   : /path           │                      │ Copy permissions    │
│                     │                      │                     │
│ Mount R/O overlay   │                      │ Mount R/W overlay   │
│ at /path            │                      │ at mergedir         │
└─────────────────────┘                      └─────────────────────┘
```

## Error Handling Strategy

### Exception Hierarchy

```
std::exception
├── std::runtime_error
│   ├── DynamicMountingException
│   │   ├── MountException
│   │   └── ConfigException
│   └── (other runtime errors)
├── std::logic_error
│   └── (programming errors)
└── Custom exceptions
    ├── BadLoopDeviceCreation
    ├── BadMountApplicationImage
    ├── BadOverlayMountPersistent
    ├── BadOverlayMountReadOnly
    ├── BadMount
    ├── BadUmount
    ├── CreateDirectoryOverlay
    ├── UBootEnvAccess
    ├── UBootEnv
    └── UBootEnvVarNotAllowedContent
```

### Error Recovery

1. **PreInit Stage**: If any early mount fails, all previously mounted filesystems are unmounted
2. **Application Mount**: Errors are caught and logged, but boot continues
3. **Overlay Mounts**: Individual failures don't stop other overlays from mounting
4. **Fallback**: If primary overlay mount fails, minimal default configuration is used

## Thread Safety

The tool is **single-threaded** by design:
- Runs during preinit before any services start
- No concurrent access to resources
- Mount operations are inherently sequential

## Resource Management

### File Descriptors
- Loop device FDs are closed after mount setup
- Config file FDs use RAII (ifstream/ofstream destructors)

### Memory
- STL containers manage their own memory
- No manual memory management required
- Smart pointers used for shared resources (e.g., UBoot handler)

## Build-time Configuration

| Define | Default | Description |
|--------|---------|-------------|
| `PATH_TO_MOUNT_APPIMAGE` | `/rw_fs/root/application/current` | Application mount point |
| `DEFAULT_OVERLAY_PATH` | `/rw_fs/root/application/current/overlay.ini` | Config file path |
| `DEFAULT_UPPERDIR_PATH` | `/rw_fs/root/upperdir` | Default upper directory base |
| `DEFAULT_WORKDIR_PATH` | `/rw_fs/root/workdir` | Default work directory base |
| `PERSISTMEMORY_REGEX_EMMC` | `root=\/dev\/(mmcblk[0-2])` | eMMC detection regex |
| `PERSISTMEMORY_REGEX_NAND` | `root=\/dev\/(ubiblock\d+_\d+)` | NAND detection regex |
| `PERSISTMEMORY_DEVICE_NAME` | `data` | Persistent partition label |
| `PERSISTENT_MEMORY_MOUNTPOINT` | `/rw_fs/root` | Persistent mount point |
