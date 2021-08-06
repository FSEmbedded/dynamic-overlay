#pragma once
// Include u-boot interface
//#include <env/fw_env.h>
// Include c++ librarys
#include <string> 
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

class INIHeaderNotFound : public std::exception
{
    private:
        std::string error_string;
    public:
        INIHeaderNotFound(const std::string & error_var)
        {
            this->error_string = std::string("Mounting application image thrown following errno: ") + error_var;
        }
        const char * what() const throw () {
            return this->error_string.c_str();
        }
};

class ApplicationImageNotCorrectDefined : public std::exception
{
    private:
        std::string error_string;
    public:
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
        DynamicMounting();
        DynamicMounting(const DynamicMounting &) = delete;
        DynamicMounting &operator=(const DynamicMounting &) = delete;
        DynamicMounting(DynamicMounting &&) = delete;
        DynamicMounting &operator=(DynamicMounting &&) = delete;
        
        void application_image();
        ~DynamicMounting();

};