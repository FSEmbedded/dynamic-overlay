/**
 * Implement the mounting of persistent and application specific overlay.
 *
 * In the header file are some specific #defines
 *
 * #define DEFAULT_OVERLAY_PATH: The standard path to the overlay.ini with mounted application image.
 * #define DEFAULT_APPLICATION_PATH: Where the application image is to be mounted.
 * #define DEFAULT_UPPERDIR_PATH: Path for the upperdir overlay directory.
 * #define DEFAULT_WORKDIR_PATH: Path to the workdir overlay directory.
 */

#pragma once

#include <exception>
#include <map>
#include <regex>
#include <vector>
#include <filesystem>

#include "mount.h"
#include "u-boot.h"

#define DEFAULT_OVERLAY_PATH "/rw_fs/root/application/current/overlay.ini"
#define DEFAULT_APPLICATION_PATH "/rw_fs/root/application/current"
#define DEFAULT_UPPERDIR_PATH "/rw_fs/root/upperdir"
#define DEFAULT_WORKDIR_PATH "/rw_fs/root/workdir"

//////////////////////////////////////////////////////////////////////////////
// Own Exceptions

class ApplicationImageNotCorrectDefined : public std::exception
{
    private:
        std::string error_string;
    public:
        /**
         * UBoot Environment variable \"application\" is wrong defined.
         */
        ApplicationImageNotCorrectDefined()
        {
            this->error_string = std::string("In u-boot environment variable \"application\" is wrong defined ");
        }
        const char * what() const throw () {
            return this->error_string.c_str();
        }
};

class ININotDefinedEntry : public std::exception
{
    private:
        std::string error_string;
    public:
        /**
         * Overlay.ini of application image has got configuration errors
         * @param wrong_ini_section Ini section that conatins error
         * @param wrong_ini_entry Ini entry that contains error
         */
        ININotDefinedEntry(const std::string & wrong_ini_section, const std::string & wrong_ini_entry)
        {
            this->error_string = std::string("INI section \"") 
                                 + wrong_ini_section
                                 + std::string("\" contains a not defined entry: ")
                                 + wrong_ini_entry;
        }
        const char * what() const throw () {
            return this->error_string.c_str();
        }
};

class ININotDefinedSection : public std::exception
{
    private:
        std::string error_string;
    public:
        /**
         * INI-Section is not defined.
         * @param wrong_ini_section Ini section that can not be found.
         */
        ININotDefinedSection(const std::string & wrong_ini_section)
        {
            this->error_string = std::string("INI section \"") 
                                 + wrong_ini_section
                                 + std::string("\" is unknown");
        }
        const char * what() const throw () {
            return this->error_string.c_str();
        }
};

class ApplicationImageAlreadyMounted : public std::exception
{
    private:
        std::string error_string;
    public:
        /**
         * Application is already mounted. Remounting is not possible and is a big error.
         */
        ApplicationImageAlreadyMounted()
        {
            this->error_string = std::string("Application image already mounted");
        }
        const char * what() const throw () {
            return this->error_string.c_str();
        }
};

//////////////////////////////////////////////////////////////////////////////

class DynamicMounting
{
    private:
        std::vector<std::string> overlay_application;
        std::map<std::string, OverlayDescription::Persistent> overlay_persistent;
        static bool application_mounted;
        const std::filesystem::path overlay_workdir, overlay_upperdir;
        const std::filesystem::path appimage_currentdir;
        const std::regex application_image_folder, persistent_memory_image;

        // private functions
        void mount_application() const;
        void read_and_parse_ini();
        void mount_overlay() const;


    public:
        /**
         * Set root upper-, work- and currentdir for persistent memory.
         * Set regex for persitent-, and application memory in .ini file.
         */
        DynamicMounting();

        DynamicMounting(const DynamicMounting &) = delete;
        DynamicMounting &operator=(const DynamicMounting &) = delete;
        DynamicMounting(DynamicMounting &&) = delete;
        DynamicMounting &operator=(DynamicMounting &&) = delete;
        
        /**
         * Handle the application image.
         * Mount application, read and parse overlay.ini.
         * Mount overlays from mounted application image.
         * @throw ApplicationImageAlreadyMounted
         */
        void application_image();

        ~DynamicMounting();
};