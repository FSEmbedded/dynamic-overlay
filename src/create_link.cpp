#include "create_link.h"
#include <stdexcept>
#include <iostream>
#include <fstream>
#include <string>
#include <regex>
#include <unistd.h>

/**
 * Creates a soft link to the system_conf file depending on the board memory type.
 * This file is used by RAUC.
 */
static void create_link_to_system_conf(const PersistentMemDetector::MemType &type, const std::string &boot_device);

/**
 * Creates a soft link to the fw_env_conf file depending on the board memory type.
 * This file is used by system tools like fw_printenv.
 */
static void create_link_to_fw_env_conf(const PersistentMemDetector::MemType &type, const std::string &boot_device);

// Precompiled regex pattern for device entries (optimized)
inline const std::regex deviceRegex(
    R"((device=)?/dev/mmcblk\d+(p\d+|boot\d+))",
    std::regex::optimize);


/**
 * Updates the RAUC system.conf or fw_env.conf files with the detected boot device
 * @param configPath Path to the system.conf file
 * @param bootDevice The detected boot device (e.g., "mmcblk0", "mmcblk1", etc.)
 * @return True if the update was successful, false otherwise
 */
static bool updateMCCBootDevConf(const std::filesystem::path& configPath, const std::string& bootDevice) {
    // Prepare temporary file path
    std::filesystem::path tmpPath = configPath;
    tmpPath += ".tmp";

    // Open input file for reading
    std::ifstream inFile(configPath, std::ios::in | std::ios::binary);
    if (!inFile.is_open()) {
        std::cerr << "Error: Cannot open " << configPath << " for reading\n";
        return false;
    }

    // Open temporary output file for writing
    std::ofstream outFile(tmpPath, std::ios::out | std::ios::trunc | std::ios::binary);
    if (!outFile.is_open()) {
        std::cerr << "Error: Cannot open " << tmpPath << " for writing\n";
        return false;
    }

    // Build replacement pattern for regex_replace
    const std::string replacement = "$1/dev/" + bootDevice + "$2";
    std::string line;

    // Process file line by line
    while (std::getline(inFile, line)) {
        // Replace all occurrences in the line
        std::string newLine = std::regex_replace(line, deviceRegex, replacement);
        outFile << newLine << '\n';
    }

    inFile.close();
    outFile.close();

    // Atomically replace original file
    std::error_code ec;
    std::filesystem::rename(tmpPath, configPath, ec);
    if (ec) {
        std::cerr << "Error renaming temp file: " << ec.message() << "\n";
        return false;
    }
    // write file to disk
    ::sync();
    // Remove the temporary file
    std::filesystem::remove(tmpPath, ec);

    return true;
}


void create_link_to_system_conf(const PersistentMemDetector::MemType &type, const std::string &boot_device)
{
    std::filesystem::path source, destination;

    if (type == PersistentMemDetector::MemType::NAND)
    {
        source = std::filesystem::path(NAND_RAUC_SYSTEM_CONF_PATH);
    }
    else
    {
        source = std::filesystem::path(EMMC_RAUC_SYSTEM_CONF_PATH);
    }

    destination = std::filesystem::path(RAUC_SYSTEM_CONF_PATH);

    try
    {
        std::filesystem::create_symlink(source, destination);
        if (type == PersistentMemDetector::MemType::eMMC)
        {
            // Update the system.conf file with the detected boot device
            updateMCCBootDevConf(destination, boot_device);
        }
    }
    catch (...)
    {
        create_link::CreateSymlink(source, destination);
    }
}

void create_link_to_fw_env_conf(const PersistentMemDetector::MemType &type, const std::string &boot_device)
{
    std::filesystem::path source, destination;

    if (type == PersistentMemDetector::MemType::NAND)
    {
        source = std::filesystem::path(NAND_UBOOT_ENV_PATH);
    }
    else
    {
        source = std::filesystem::path(EMMC_UBOOT_ENV_PATH);
    }

    destination = std::filesystem::path(UBOOT_ENV_PATH);

    try
    {
        std::filesystem::create_symlink(source, destination);
        // Check if the destination is a symlink
        if (type == PersistentMemDetector::MemType::eMMC)
        {
            // Update the fw_env.conf file with the detected boot device
            updateMCCBootDevConf(source, boot_device);
        }
    }
    catch (...)
    {
        create_link::CreateSymlink(source, destination);
    }
}

OverlayDescription::ReadOnly create_link::prepare_ramdisk(const std::filesystem::path & path_to_ramdisk,
    const PersistentMemDetector::MemType &mem_type,
    const std::string &boot_device)
{
    if (!std::filesystem::exists(path_to_ramdisk))
    {
        if (!std::filesystem::create_directories(path_to_ramdisk))
        {
            throw(CreateRAMfsMountpoint(path_to_ramdisk));
        }
    }

    Mount mount;

    mount.wrapper_c_mount(
        "none",
        path_to_ramdisk,
        "size=16M,mode=0755",
        "tmpfs",
        0
    );

    OverlayDescription::Persistent ramdisk;
    ramdisk.lower_directory = "/etc";
    ramdisk.merge_directory = "/etc";

    ramdisk.upper_directory = path_to_ramdisk;
    ramdisk.upper_directory += "/upper/etc";

    ramdisk.work_directory = path_to_ramdisk;
    ramdisk.work_directory += "/work/etc";

    mount.mount_overlay_persistent(ramdisk);


    create_link_to_system_conf(mem_type, boot_device);
    create_link_to_fw_env_conf(mem_type, boot_device);

    //mount.wrapper_c_umount("/etc");
    /* remount /etc as read-only */
    mount.wrapper_c_mount(
        "none",
        ramdisk.merge_directory,
        "",
        "",
        MS_REMOUNT|MS_RDONLY
    );

    OverlayDescription::ReadOnly ramdisk_ro;
    ramdisk_ro.lower_directory = ramdisk.lower_directory;
    ramdisk_ro.merge_directory = ramdisk.merge_directory;
//    ramdisk_ro.work_directory = ramdisk.work_directory;

    return ramdisk_ro;
}

std::filesystem::path create_link::get_fw_env_config(const PersistentMemDetector::MemType &mem_type)
{
    if (mem_type == PersistentMemDetector::MemType::NAND)
    {
        return std::filesystem::path(NAND_UBOOT_ENV_PATH);
    }
    else if (mem_type == PersistentMemDetector::MemType::eMMC)
    {
        return std::filesystem::path(EMMC_UBOOT_ENV_PATH);
    }
    else
    {
        throw(std::logic_error("Memory type is not defined"));
    }
}
