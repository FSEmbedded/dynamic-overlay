# Component Architecture

Module dependency diagram. Arrows point from caller to dependency.

```
┌──────────────────────────────────────────────────────────────────┐
│                           main.cpp                               │
│                        (Entry Point)                             │
└──────────────────────────────────────────────────────────────────┘
                                │
         ┌──────────────────────┼──────────────────────┐
         │                      │                      │
         ▼                      ▼                      ▼
┌────────────────┐    ┌─────────────────┐    ┌────────────────┐
│    PreInit     │    │ PersistentMem   │    │     UBoot      │
│                │    │    Detector     │    │                │
│ Mount /proc    │    │                 │    │ Read vars      │
│ Mount /sys     │    │ Detect eMMC    │    │ Validate       │
│ Mount data     │    │ Detect NAND    │    │ Type convert   │
└────────────────┘    └─────────────────┘    └────────────────┘
         │                      │                      │
         └──────────────────────┼──────────────────────┘
                                │
                                ▼
┌──────────────────────────────────────────────────────────────────┐
│                       DynamicMounting                            │
│                                                                  │
│ Parse overlay.ini                                                │
│ Manage overlay lists (ApplicationFolder, PersistentMemory)       │
│ Orchestrate mount sequence                                       │
│ Handle A/B slot selection                                        │
│ Detect failed updates                                            │
└──────────────────────────────────────────────────────────────────┘
                                │
                                ▼
┌──────────────────────────────────────────────────────────────────┐
│                            Mount                                 │
│                                                                  │
│ mount_application_image()   : Loop device + squashfs mount       │
│ mount_overlay_persistent()  : R/W overlay with upperdir          │
│ mount_overlay_readonly()    : R/O overlay (multiple lowerdirs)   │
│ wrapper_c_mount()           : Generic mount syscall wrapper      │
│ wrapper_c_umount()          : Generic umount syscall wrapper     │
└──────────────────────────────────────────────────────────────────┘
                                │
              ┌─────────────────┼─────────────────┐
              │                 │                 │
              ▼                 ▼                 ▼
┌──────────────────┐  ┌──────────────────┐  ┌──────────────────┐
│ file_properties  │  │   create_link    │  │ x509_cert_store  │
│                  │  │                  │  │   (optional)     │
│ Copy permissions │  │ Generate configs │  │                  │
│ Copy xattrs      │  │ Rewrite device   │  │ Extract to tmpfs │
│ Validate props   │  │ paths            │  │ ScopeGuard       │
└──────────────────┘  └──────────────────┘  └──────────────────┘

Shared foundation (used by all modules above):

┌──────────┐  ┌──────────┐  ┌──────────────┐  ┌──────────────┐
│ error.h  │  │logging.h │  │ posix_utils  │  │ string_utils │
│          │  │          │  │              │  │              │
│ ABI enum │  │ /dev/kmsg│  │ FdGuard      │  │ trim, split  │
│ 34 codes │  │ compile  │  │ ScopeGuard   │  │ join, lower  │
│          │  │ out      │  │ DirGuard     │  │              │
└──────────┘  └──────────┘  └──────────────┘  └──────────────┘

┌──────────────┐  ┌──────────────┐  ┌──────────────────┐
│ ini_parser   │  │device_parser │  │  overlay_config  │
│              │  │              │  │                  │
│ INI → map    │  │ /proc/cmdline│  │ PersistentMemory │
│ ordered keys │  │ root= parser │  │ section parser,  │
│              │  │              │  │ nosuid schema    │
└──────────────┘  └──────────────┘  └──────────────────┘
```
