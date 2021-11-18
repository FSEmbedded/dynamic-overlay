#include"mount.h"

// Icnludes for kernel functions mount
extern "C" {
    #include <sys/mount.h>
    #include <fcntl.h>
    #include <linux/loop.h>
    #include <sys/ioctl.h>
    #include <stdio.h>
    #include <stdlib.h>
    #include <unistd.h>
}

#include <cerrno>
#include <filesystem>

Mount::Mount():
    path_to_container(PATH_TO_MOUNT_APPIMAGE)
{

}

Mount::~Mount()
{

}

void Mount::mount_application_image(const std::string & pathToImage) const
{   
    int loopctlfd, loopfd, backingfile;
    long devnr;
    
    loopctlfd = open("/dev/loop-control", O_RDWR);
    if (loopctlfd == -1)
    {
        throw(BadLoopDeviceCreation(errno, std::string("open: /dev/loop-control")));
    }

    devnr = ioctl(loopctlfd, LOOP_CTL_GET_FREE);
    if (devnr == -1)
    {
        close(loopctlfd);
        throw(BadLoopDeviceCreation(errno, std::string("ioctl-LOOP_CTL_GET_FREE")));
    }

    std::string loopname = std::string("/dev/loop") + std::to_string(devnr);

    loopfd = open(loopname.c_str(), O_RDWR);
    if (loopfd == -1)
    {
        ioctl(loopctlfd, LOOP_CTL_REMOVE, devnr);
        close(loopctlfd);
        throw(BadLoopDeviceCreation(errno, std::string("Cannot open: \"") + loopname + std::string("\"")));
    }

    backingfile = open(pathToImage.c_str(), O_RDWR);
    if (backingfile == -1)
    {
        close(loopfd);
        ioctl(loopctlfd, LOOP_CTL_REMOVE, devnr);
        close(loopctlfd);
        throw(BadLoopDeviceCreation(errno, std::string("Could not open: \"") + pathToImage + std::string("\" image")));
    }

    if (ioctl(loopfd, LOOP_SET_FD, backingfile) == -1)
    {
        close(backingfile);
        close(loopfd);
        ioctl(loopctlfd, LOOP_CTL_REMOVE, devnr);
        close(loopctlfd);
        const std::string error = std::string("Cannot mount: \"") + loopname
                                  + std::string("\" on \"") + pathToImage;
        throw(BadLoopDeviceCreation(errno, error));
    }
    
    const int mount_state = mount(  loopname.c_str(), 
                                    this->path_to_container.c_str(),
                                    "squashfs", 0,
                                    NULL);
    if (mount_state != 0){
        
        ioctl(loopfd, LOOP_CLR_FD, backingfile);
        close(backingfile);
        close(loopfd);
        ioctl(loopctlfd, LOOP_CTL_REMOVE, devnr);
        close(loopctlfd);
        throw(BadMountApplicationImage(errno));
    }
    close(loopfd);
    close(loopctlfd);
    close(backingfile);

}

void Mount::mount_overlay_persistent(const OverlayDescription::Persistent & container) const
{
    if (!std::filesystem::exists(container.upper_directory))
    {
        if (!std::filesystem::create_directories(container.upper_directory))
        {
            throw(CreateDirectoryOverlay(container.upper_directory));
        }
    }

    if (!std::filesystem::exists(container.work_directory))
    {
        if (!std::filesystem::create_directories(container.work_directory))
        {
            throw(CreateDirectoryOverlay(container.work_directory));
        }
    }

    const std::string mount_args =  std::string("upperdir=") + std::string(container.upper_directory) + std::string(",") +
                                    std::string("workdir=") + std::string(container.work_directory) + std::string(",") +
                                    std::string("lowerdir=") + std::string(container.lower_directory);

    const int mount_state = mount(  "overlay",
                                    container.merge_directory.c_str(),
                                    "overlay", 0,
                                    mount_args.c_str());
    if (mount_state != 0){
        throw BadOverlayMountPersistent(errno, container);
    }
}

void Mount::mount_overlay_readonly(const OverlayDescription::ReadOnly & container) const
{
    const std::string mount_args(std::string("lowerdir=") + container.lower_directory);

    const int mount_state = mount(  "overlay",
                                    container.merge_directory.c_str(),
                                    "overlay", 0,
                                    mount_args.c_str());
    if (mount_state != 0){
        throw BadOverlayMountReadOnly(errno, container);
    }
}

void Mount::wrapper_c_mount( const std::string &memory_device,
                const std::string &dest_dir,
                const std::string &options,
                const std::string &filesystem,
                const unsigned long &flag)
{
    int mount_state;

    const char * ptr_source_str = memory_device.c_str();

    const char * ptr_options_str = options.c_str();
    if ( options.length() == 0 )
    {
        ptr_options_str = nullptr;
    }

    const char * ptr_dest_str = dest_dir.c_str();
    const char * ptr_filesystem_str = filesystem.c_str();

    mount_state = mount(
        ptr_source_str,
        ptr_dest_str,
        ptr_filesystem_str,
        flag,
        ptr_options_str
    );

    if (mount_state != 0)
    {
        throw(BadMount(memory_device, errno));
    }
}

void Mount::wrapper_c_umount(const std::string &path)
{
    const int umount_state = umount(path.c_str());
    
    if (umount_state != 0)
    {
        throw(BadUmount(path, errno));
    }
}

