#pragma once
#include <exception>
#include <cerrno>
#include <clocale>
#include <cstring>
#include <string>
#include <vector>
#include <iostream>
extern "C"
{
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/xattr.h>
}
#include "mount.h"

namespace file_properties
{
    //////////////////////////////////////////////////////////////////////////////
    // Own Exceptions
    class ErrnoCstat : public std::exception
    {
    private:
        std::string error_string;

    public:
        /**
         * Map ERRNO to error string of c-function stat.
         * @param local_errno Errno variable of stat c-function.
         * @param path Path which failed.
         */
        ErrnoCstat(const int &local_errno, const std::string &path)
        {
            std::setlocale(LC_MESSAGES, "en_EN.utf8");
            this->error_string = std::string("c-function stat failed with: ") + std::string(std::strerror(local_errno));
            this->error_string += std::string("; path: ") + path;
        }

        const char *what() const noexcept override
        {
            return this->error_string.c_str();
        }
    };

    class ErrnoCchmod : public std::exception
    {
    private:
        std::string error_string;

    public:
        /**
         * Map ERRNO to error string of c-function chmod.
         * @param local_errno Errno variable of chmod c-function.
         * @param path Path which failed.
         */
        ErrnoCchmod(const int &local_errno, const std::string &path)
        {
            std::setlocale(LC_MESSAGES, "en_EN.utf8");
            this->error_string = std::string("c-function chmod failed with: ") + std::string(std::strerror(local_errno));
            this->error_string += std::string("; path: ") + path;
        }

        const char *what() const noexcept override
        {
            return this->error_string.c_str();
        }
    };

    class ErrnoCchown : public std::exception
    {
    private:
        std::string error_string;

    public:
        /**
         * Map ERRNO to error string of c-function chown.
         * @param local_errno Errno variable of chown c-function.
         * @param path Path which failed.
         */
        ErrnoCchown(const int &local_errno, const std::string &path)
        {
            std::setlocale(LC_MESSAGES, "en_EN.utf8");
            this->error_string = std::string("c-function chown failed with: ") + std::string(std::strerror(local_errno));
            this->error_string += std::string("; path: ") + path;
        }

        const char *what() const noexcept override
        {
            return this->error_string.c_str();
        }
    };

    /**
     * Compare the upper and system directory, if the same owner, and rights are set.
     * This function compares the system directory (last in lowerdir) with the upper directory
     * to ensure correct permissions are applied.
     *
     * @param overlay Persistent overlay data structure.
     * @return true Both directories have the same permissions and owner.
     * @return false Otherwise.
     */
    [[nodiscard]] bool properties_set(const OverlayDescription::Persistent &overlay);

    /**
     * Copy the permissions and owner from the system directory and change the permissions of the upper directory.
     * This ensures that the upper directory has the correct system permissions (root:root) instead of
     * the potentially incorrect permissions from the user-created application squashfs.
     *
     * @param overlay Persistent overlay data structure.
     * @throws ErrnoCstat if stat fails on the system directory.
     * @throws ErrnoCchmod if chmod fails on the upper directory.
     * @throws ErrnoCchown if chown fails on the upper directory.
     */
    void copy_properties_lower_to_upper(const OverlayDescription::Persistent &overlay);

    /**
     * Helper function to extract the system (last) lower directory from a colon-separated lowerdir string.
     * In the overlay setup: /app/current/etc:/etc
     * We want the system directory (/etc) not the application directory.
     *
     * @param lowerdir_string Colon-separated string of lower directories
     * @return System directory path (last directory in the list)
     */
    [[nodiscard]] std::string get_system_lower_directory(const std::string &lowerdir_string);

    /**
     * Copies extended attributes from source to target directory.
     *
     * @param source_dir Source directory path
     * @param target_dir Target directory path
     */
    void copy_extended_attributes(const std::string &source_dir, const std::string &target_dir);

    /**
     * Copies a single extended attribute from source to target directory.
     *
     * @param source_dir Source directory path
     * @param target_dir Target directory path
     * @param attr_name Name of the attribute to copy
     */
    void copy_single_attribute(const std::string &source_dir, const std::string &target_dir, const char *attr_name);
}
