#pragma once

#include <string>
#include <exception>

extern "C" {
    #include <sys/mount.h>
}

#define PATH_TO_MOUNT_APPIMAGE "/rw_fs/root/application/current"

/**
 * Abstract mount c-interface and add the functionality to mount persistent and read-only overlay folder.
 *
 * Abstract the c-interface, also add some specific feature for mounting the persistent memory and 
 * application image related folders.
 *
 * #define PATH_TO_MOUNT_APPIMAGE: Default path to mount application image.
 */
namespace OverlayDescription
{
    //////////////////////////////////////////////////////////////////////////////
    // Data Class
    class Persistent{
        public:
            std::string lower_directory, work_directory, merge_directory, upper_directory;

            Persistent(){}

            Persistent(const Persistent & source){
                this->lower_directory = source.lower_directory;
                this->work_directory = source.work_directory;
                this->merge_directory = source.merge_directory;
                this->upper_directory = source.upper_directory;
            }

            Persistent(Persistent && source):
                lower_directory(std::move(source.lower_directory)),
                work_directory(std::move(source.work_directory)),
                merge_directory(std::move(source.merge_directory)),
                upper_directory(std::move(source.upper_directory))
            {

            }
            
            Persistent &operator=(const Persistent &source)
            {
                this->lower_directory = source.lower_directory;
                this->work_directory = source.work_directory;
                this->merge_directory = source.merge_directory;
                this->upper_directory = source.upper_directory;
                return *this;
            }

            ~Persistent(){}
    };

    class ReadOnly{
        public:
            std::string lower_directory, merge_directory;

            ReadOnly(){}

            ReadOnly(const ReadOnly & source){
                this->lower_directory = source.lower_directory;
                this->merge_directory = source.merge_directory;
            }

            ReadOnly(ReadOnly && source):
                lower_directory(std::move(source.lower_directory)),
                merge_directory(std::move(source.merge_directory))
            {

            }

            ReadOnly &operator=(const ReadOnly &source)
            {
                this->lower_directory = source.lower_directory;
                this->merge_directory = source.merge_directory;
                return *this;
            }

            ~ReadOnly(){}
    };
};

//////////////////////////////////////////////////////////////////////////////
// Own Exceptions

class BadLoopDeviceCreation : public std::exception
{
    private:
        std::string error_string;
    public:
        /**
         * Error during creating a loop-device for mounting an application image.
         * @error_var Copy of errno during execution.
         * @error_str Step of execution.
         */
        BadLoopDeviceCreation(const int &error_var, const std::string &error_str)
        {
            this->error_string = std::string("Creating loop device throw errno: \"") + std::to_string(error_var);
            this->error_string += std::string("\" while: \"") + error_str;
            this->error_string += std::string("\"");
        }
        const char * what() const throw () {
            return this->error_string.c_str();
        }
};

class BadMountApplicationImage : public std::exception
{
    private:
        std::string error_string;
    public:
        /**
         * Mounting of application image failed.
         * @param error_var Copy of errno during execution.
         */
        BadMountApplicationImage(const int & error_var)
        {
            this->error_string = std::string("Mounting application image thrown following errno: ") + std::to_string(error_var);
        }
        const char * what() const throw () {
            return this->error_string.c_str();
        }
};

class BadOverlayMountPersistent : public std::exception
{
    private:
        std::string error_string;
    public:
        const OverlayDescription::Persistent mount_args;
        /**
         * Can not mount overlay-filesystem for persistent memory.
         * @param error_var Copy of errno during execution.
         * @param mount_args OverlayDescription::Persistent object which failed.
         */
        BadOverlayMountPersistent(const int & error_var, const OverlayDescription::Persistent & mount_args): mount_args(mount_args)
        {
            this->error_string = std::string("Mounting overlay failed with: ") + std::to_string(error_var);
        }
        const char * what() const throw ()
        {
            return this->error_string.c_str();
        }
};

class BadOverlayMountReadOnly : public std::exception
{
    private:
        std::string error_string;
    public:
        const OverlayDescription::ReadOnly mount_args;
        /**
         * Can not mount overlay-filesystem for application image.
         * @param error_var Copy of errno during execution.
         * @param mount_args OverlayDescription::ReadOnly object which failed.
         */
        BadOverlayMountReadOnly(const int & error_var, const OverlayDescription::ReadOnly & mount_args): mount_args(mount_args)
        {
            this->error_string = std::string("Mounting overlay failed with: ") + std::to_string(error_var);
        }
        const char * what() const throw ()
        {
            return this->error_string.c_str();
        }
};

class CreateDirectoryOverlay : public std::exception
{
    private:
        std::string error_string;
    public:
        /**
         * Can not create directories of persistent memory which are used for overlay-fs.
         * @param dir Directory path which can not be created.
         */
        CreateDirectoryOverlay(const std::string & dir)
        {
            this->error_string = std::string("Mounting application image thrown following errno: ") + dir;
        }
        const char * what() const throw () {
            return this->error_string.c_str();
        }
};

class BadMount : public std::exception
{
    private:
        std::string error_string;
    public:
        /**
         * Could not mount given memory device.
         * @param destr_dir Path to memory-device.
         * @param loc_errno Copy of errno given during failed execution.
         */
        BadMount(const std::string &destr_dir, const int loc_errno)
        {
            this->error_string = std::string("Mount of \"") + destr_dir;
            this->error_string += std::string("\" failed with errno: ") + std::to_string(loc_errno);
        }

        const char * what() const throw () {
            return this->error_string.c_str();
        }
};

class BadUmount : public std::exception
{
    private:
        std::string error_string;
    public:
        /**
         * Could not umount given mounted folder.
         * @param destr_dir Path to failed folder to umount.
         * @param loc_errno Copy of errno given during failed execution.
         */
        BadUmount(const std::string &destr_dir, const int loc_errno)
        {
            this->error_string = std::string("Umount of \"") + destr_dir;
            this->error_string += std::string("\" failed with errno: ") + std::to_string(loc_errno);
        }

        const char * what() const throw () {
            return this->error_string.c_str();
        }
};
//////////////////////////////////////////////////////////////////////////////
// Main Class

class Mount
{
    private:
        const std::string path_to_container;
    public:

        Mount();

        Mount(const Mount &) = delete;
        Mount &operator=(const Mount &) = delete;
        Mount(Mount &&) = delete;
        Mount &operator=(Mount &&) = delete;

        /**
         * Mount application image to the standard mounting point in image.
         * It is not forbidden to call this step multiple times, but well it make no sense.
         * @param pathToImage Path to application image that should be mounted
         * @throw BadLoopDeviceCreation Error during interaction with linux kernel.
         * @throw BadMountApplicationImage Error during mount process.
         */
        void mount_application_image(const std::string &) const;

        /**
         * Mount OverlayDescription::Persistent as an overlay on the current filesystem.
         * Destination of mount point is fixed and needed directories will be created automatically.
         * @param container DataClass object which contain all needed parameters.
         * @throw CreateDirectoryOverlay Can not create directory for overlay.
         * @throw BadOverlayMountPersistent Can not mount persistent memory.
         */
        void mount_overlay_persistent(const OverlayDescription::Persistent &) const;

        /**
         * Mount OverlayDescription::ReadOnly as an overlay on the current filesystem.
         * Destination of mount point is fixed and needed directories will be created automatically.
         * @param container DataClass object which contain all needed parameters.
         * @throw BadOverlayMountReadOnly Can not mount read-only directories.
         */
        void mount_overlay_readonly(const OverlayDescription::ReadOnly &) const;

        /**
         * Wrapper method of mount c-function.
         * @param memory_device Source memory device, the root source.
         * @param dest_dir Destination mapping for root memory device.
         * @param options Mount options, keep empty when no options are set.
         * @param filesystem Filesystem type.
         * @param flag Specify the mount actions: flag=0 means new mount without any special treatment.
         * @throw BadMount Error during mount.
         */
        void wrapper_c_mount(const std::string &memory_device,
                        const std::string &dest_dir,
                        const std::string &options,
                        const std::string &filesystem,
                        const unsigned long &flag);
        /**
         * Wrapper method of umount c-function.
         * @param path Path to mounted directory.
         * @throw BadUmount Umount is not possible.
         */
        void wrapper_c_umount(const std::string &);

        ~Mount();
};
