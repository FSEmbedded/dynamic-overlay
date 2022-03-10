#pragma once

#include <exception>
#include <cerrno>
#include <clocale>
#include <cstring>

extern "C" {
    #include <sys/types.h>
    #include <sys/stat.h>
    #include <unistd.h>
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
            const char * what() const throw () {
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
            const char * what() const throw () {
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
             * @param local_errno Errno variable of chmod c-function.
             * @param path Path which failed.
             */
            ErrnoCchown(const int &local_errno, const std::string &path)
            {
                std::setlocale(LC_MESSAGES, "en_EN.utf8");
                this->error_string = std::string("c-function chown failed with: ") + std::string(std::strerror(local_errno));
                this->error_string += std::string("; path: ") + path;
            }
            const char * what() const throw () {
                return this->error_string.c_str();
            }
    };

    /**
     * Compare the upper and lower directory, if the same owner, and rights are set.
     *
     * @param overlay Persistent overlay data structure.
     * @return true Both folder have the same permissions and owner.
     * @return false
     */
    bool properties_set(const OverlayDescription::Persistent &overlay);

    /**
     * Copy the permissions and owner from the lower directory and change the permissions from the upper directory.
     *
     * @param overlay Persistent overlay data structure.
     */
    void copy_properties_lower_to_upper(const OverlayDescription::Persistent &overlay);

}
