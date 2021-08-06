#pragma once

#include <string>
#include <exception>
#include <filesystem>

#define PATH_TO_MOUNT_APPIMAGE "/rw_fs/root/application/current"

//////////////////////////////////////////////////////////////////////////////
// Data Class

namespace OverlayDescription
{
    class Persistent{
        public:
            std::filesystem::path lower_directory, work_directory, merge_directory, upper_directory;

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
            std::filesystem::path lower_directory, merge_directory;

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
        const OverlayDescription::Persistent mount_args;
    public:
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
        const OverlayDescription::ReadOnly mount_args;
    public:
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
        CreateDirectoryOverlay(const std::filesystem::path & dir)
        {
            this->error_string = std::string("Mounting application image thrown following errno: ") + dir.string();
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
        Mount(const Mount &) = delete;
        Mount &operator=(const Mount &) = delete;
        Mount(Mount &&) = delete;
        Mount &operator=(Mount &&) = delete;

        Mount();
        void mount_application_image(const std::string &) const;
        void mount_overlay_persistent(const OverlayDescription::Persistent &) const;
        void mount_overlay_readonly(const OverlayDescription::ReadOnly &) const;
        void wrapper_c_mount(const std::filesystem::path &source_dir,
                        const std::filesystem::path &dest_dir,
                        const std::string &options,
                        const std::string &filesystem,
                        const unsigned long &flag);
        void wrapper_c_umount(const std::filesystem::path &);

        ~Mount();
};
