# F&S Dynamic Mounting

__Dynamic Mounting__ is a part of the __F&S Update Framework__ and handle the __application image__.

## How it works

The dynamic overlay mounts the application before systemd or any other kind of init systems starts. It is running as a preinit.

1. Prepare the enviroment and mount __proc__ and __sys__.

2. Mount __persistent__ memory partition.

3. Create the __loop0__ device and mount application image depending on the UBoot variable __application__.

4. Parse __overlay.ini__ and mount all mentioned __overlays__.

5. Umount __proc__ and __sys__ for starting systemd or any other kind of init-system.


After preparation the normal boot process will proceed and work on the overlay filesystem as normal root filesystem.

## Dependencies

[libubootenv-0.3.2](https://github.com/sbabic/libubootenv)

[libbotan-2.18.1](https://github.com/randombit/botan)

[zlib-1.2.11](http://zlib.net/)
