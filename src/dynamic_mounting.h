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
#include <list>
#include <memory>

// Forward declarations
class UBoot;
namespace inicpp
{
    class section;
}

#include "mount.h"
#include "u-boot.h"

#define DEFAULT_OVERLAY_PATH "/rw_fs/root/application/current/overlay.ini"
#define DEFAULT_APPLICATION_PATH "/rw_fs/root/application/current"
#define DEFAULT_UPPERDIR_PATH "/rw_fs/root/upperdir"
#define DEFAULT_WORKDIR_PATH "/rw_fs/root/workdir"

class DynamicMountingException : public std::runtime_error
{
public:
    explicit DynamicMountingException(const std::string &message)
        : std::runtime_error("DynamicMounting error: " + message) {}
};

class MountException : public DynamicMountingException
{
public:
    explicit MountException(const std::string &message)
        : DynamicMountingException("Mount error: " + message) {}
};

class ConfigException : public DynamicMountingException
{
public:
    explicit ConfigException(const std::string &message)
        : DynamicMountingException("Configuration error: " + message) {}
};

class DynamicMounting
{
private:
    std::vector<std::string> overlay_application;
    std::map<std::string, OverlayDescription::Persistent> overlay_persistent;
    static bool application_mounted;
    const std::string overlay_workdir, overlay_upperdir;
    const std::string appimage_currentdir;
    const std::regex application_image_folder, persistent_memory_image;
    std::shared_ptr<UBoot> uboot_handler;

    std::list<OverlayDescription::ReadOnly> additional_lower_directory_to_persistent;
    std::vector<std::list<OverlayDescription::ReadOnly>::iterator> used_entries_application_overlay;

    // private functions
    void mount_application() const;
    void read_and_parse_ini();
    void mount_overlay_read_only(bool application_mounted_overlay_parsed);
    void mount_overlay_persistent();
    bool detect_failedUpdate_app_fw_reboot() const;
    /**
     * Determines which application image should be mounted
     * @return String containing the filename of the application image to mount
     */
    std::string determine_application_image() const;

    /**
     * Parses an ApplicationFolder section from the configuration
     * @param section The section to parse
     * @throws ConfigException if parsing fails
     */
    void parse_application_folder_section(const inicpp::section &section);

    /**
     * Parses a PersistentMemory section from the configuration
     * @param section The section to parse
     * @throws ConfigException if parsing fails
     */
    void parse_persistent_memory_section(const inicpp::section &section);

public:
    /**
     * Set root upper-, work- and currentdir for persistent memory.
     * Set regex for persitent-, and application memory in .ini file.
     */
    DynamicMounting(const std::shared_ptr<UBoot> &);
    ~DynamicMounting();

    DynamicMounting(const DynamicMounting &) = delete;
    DynamicMounting &operator=(const DynamicMounting &) = delete;
    DynamicMounting(DynamicMounting &&) = delete;
    DynamicMounting &operator=(DynamicMounting &&) = delete;

    /**
     * Handle the application image.
     * Mount application, read and parse overlay.ini.
     * Mount overlays from mounted application image.
     * Mount added ReadOnly object independent of application_image.
     * @throw ApplicationImageAlreadyMounted
     * @throw ApplicationImageNotCorrectDefined
     * @throw ININotDefinedSection
     * @throw ININotDefinedEntry
     */
    void application_image();

    /**
     * In this function ReadOnly memory container are added to be stacked on the read-only memory segement.
     * In Linux only 2 overlays can be stacked, but with multiple lower directories this can be bypassed.
     * e.g.: lowerdir=/lower1:/lower2:/lower3
     * This process is not limitited against multiple mount overlay commands.
     * The overlay process is started from the right to the left.
     * In this case lower3 overlays lower2 and this lower1.
     * If no lowerdir ist provided in application container, will only mount the addition.
     * Error during mounting application and persistent memory will not prohibit given ReadOnly object.
     */
    void add_lower_dir_readonly_memory(const OverlayDescription::ReadOnly &);
};
