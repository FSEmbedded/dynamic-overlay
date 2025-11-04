#include "create_link.h"
#include <stdexcept>
#include <iostream>
#include <fstream>
#include <string>
#include <regex>
#include <unistd.h>

namespace create_link
{
    // Default paths for U-Boot environment and RAUC system configuration
    constexpr const char *DEFAULT_UBOOT_ENV_PATH = "/rw_fs/root/conf/fw_env.config";
    constexpr const char *DEFAULT_RAUC_SYSTEM_CONF_PATH = "/rw_fs/root/conf/system.conf";
}

#ifndef UBOOT_ENV_PATH
#define UBOOT_ENV_PATH "/rw_fs/root/conf/fw_env.config"
#endif

#ifndef RAUC_SYSTEM_CONF_PATH
#define RAUC_SYSTEM_CONF_PATH "/rw_fs/root/conf/system.conf"
#endif



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
static bool updateMCCBootDevConf(const std::filesystem::path &configPath, const std::string &bootDevice)
{
    // Prepare temporary file path
    std::filesystem::path tmpPath = configPath;
    tmpPath += ".tmp";

    // Open input file for reading
    std::ifstream inFile(configPath, std::ios::in | std::ios::binary);
    if (!inFile.is_open())
    {
        std::cerr << "Error: Cannot open " << configPath << " for reading\n";
        return false;
    }

    // Open temporary output file for writing
    std::ofstream outFile(tmpPath, std::ios::out | std::ios::trunc | std::ios::binary);
    if (!outFile.is_open())
    {
        std::cerr << "Error: Cannot open " << tmpPath << " for writing\n";
        return false;
    }

    // Build replacement pattern for regex_replace
    const std::string replacement = "$1/dev/" + bootDevice + "$2";
    std::string line;

    // Process file line by line
    while (std::getline(inFile, line))
    {
        // Replace all occurrences in the line
        std::string newLine = std::regex_replace(line, deviceRegex, replacement);
        outFile << newLine << '\n';
    }

    inFile.close();
    outFile.close();

    // Atomically replace original file
    std::error_code ec;
    std::filesystem::rename(tmpPath, configPath, ec);
    if (ec)
    {
        std::cerr << "Error renaming temp file: " << ec.message() << "\n";
        return false;
    }
    // write file to disk
    ::sync();
    // Remove the temporary file
    std::filesystem::remove(tmpPath, ec);

    return true;
}

static bool updateMTDBootDevConf(const std::filesystem::path &configPath, const std::string &mtdDevice)
{
    // Prepare temporary file path
    std::filesystem::path tmpPath = configPath;
    tmpPath += ".tmp";

    // Open input file for reading
    std::ifstream inFile(configPath, std::ios::in | std::ios::binary);
    if (!inFile.is_open())
    {
        std::cerr << "Error: Cannot open " << configPath << " for reading\n";
        return false;
    }

    // Open temporary output file for writing
    std::ofstream outFile(tmpPath, std::ios::out | std::ios::trunc | std::ios::binary);
    if (!outFile.is_open())
    {
        std::cerr << "Error: Cannot open " << tmpPath << " for writing\n";
        return false;
    }

    // Build replacement pattern for regex_replace
    const std::string replacement = "$1/dev/" + mtdDevice + "$2";
    std::string line;
    // Regex pattern for MTD device entries
    const std::regex mtdDeviceRegex(
        R"((device=)?/dev/mtd\d+(p\d+)?)",
        std::regex::optimize);

    // Process file line by line
    while (std::getline(inFile, line))
    {
        // Replace all occurrences in the line
        std::string newLine = std::regex_replace(line, mtdDeviceRegex, replacement);
        outFile << newLine << '\n';
    }

    inFile.close();
    outFile.close();

    // Atomically replace original file
    std::error_code ec;
    std::filesystem::rename(tmpPath, configPath, ec);
    if (ec)
    {
        std::cerr << "Error renaming temp file: " << ec.message() << "\n";
        return false;
    }
    // write file to disk
    ::sync();
    // Remove the temporary file
    std::filesystem::remove(tmpPath, ec);

    return true;
}

/**
 * Finds the MTD device corresponding to the given name by parsing /proc/mtd
 * @param name The name of the MTD device to find (e.g., "UBootEnv")
 * @return The MTD device identifier (e.g., "mtd0")
 */
static std::string findMTDDeviceByName(const std::string &name)
{
    std::ifstream mtdFile("/proc/mtd");
    if (!mtdFile.is_open())
    {
        std::cerr << "Error: Cannot open /proc/mtd for reading\n";
        return "";
    }

    std::string line;
    std::regex mtdRegex(R"(^(mtd\d+): .+ \"(.+)\"$)");
    while (std::getline(mtdFile, line))
    {
        std::smatch match;
        if (std::regex_search(line, match, mtdRegex))
        {
            if (match.size() == 3 && match[2] == name)
            {
                return match[1];
            }
        }
    }
    return "";
}

void create_link::create_link_to_system_conf(const PersistentMemDetector::MemType &type, const std::string &boot_device)
{
    std::filesystem::path source, destination;

    source = get_system_conf(type);
    destination = std::filesystem::path(RAUC_SYSTEM_CONF_PATH);

    try
    {
        // copy file if not already present
        if (!std::filesystem::exists(destination))
        {
            std::filesystem::copy_file(source, destination, std::filesystem::copy_options::overwrite_existing);
        }
        // check if the destination directory exists
        // TODO: MTD devices...
        if (type == PersistentMemDetector::MemType::eMMC)
        {
            if(!isBootDeviceConfigured(destination, boot_device))
            {
                // Update the system.conf file with the detected boot device
                updateMCCBootDevConf(destination, boot_device);
            }
            // TODO: changes in mtd layout must be suitable to system.conf
        }
    }
    catch (...)
    {
        create_link::CreateSymlink(source, destination);
    }
}

void create_link::create_link_to_fw_env_conf(const PersistentMemDetector::MemType &type, const std::string &boot_device)
{
    std::filesystem::path source, destination;

    source = get_fw_env_config(type);
    destination = std::filesystem::path(UBOOT_ENV_PATH);

    try
    {
        // check if the destination directory exists
        if (!std::filesystem::exists(destination.parent_path()))
        {
            std::filesystem::create_directories(destination.parent_path());
        }
        // copy file if not already present
        if (!std::filesystem::exists(destination))
        {
            std::filesystem::copy_file(source, destination, std::filesystem::copy_options::overwrite_existing);
        }
        // Check if the destination is a symlink
        // TODO: MTD devices...
        if (type == PersistentMemDetector::MemType::eMMC)
        {
            if(!isBootDeviceConfigured(destination, boot_device))
            {
                // Update the fw_env.conf file with the detected boot device
                updateMCCBootDevConf(destination, boot_device);
            }
        } else if (type == PersistentMemDetector::MemType::NAND)
        {
            const std::string mtdDevice = findMTDDeviceByName("UBootEnv");
            if (!mtdDevice.empty() && !isBootDeviceConfigured(destination, mtdDevice))
            {
                updateMTDBootDevConf(destination, mtdDevice);
            }
        }
    }
    catch (...)
    {
        create_link::CreateSymlink(source, destination);
    }
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

std::filesystem::path create_link::get_system_conf(const PersistentMemDetector::MemType &mem_type)
{
    if (mem_type == PersistentMemDetector::MemType::NAND)
    {
        return std::filesystem::path(NAND_RAUC_SYSTEM_CONF_PATH);
    }
    else if (mem_type == PersistentMemDetector::MemType::eMMC)
    {
        return std::filesystem::path(EMMC_RAUC_SYSTEM_CONF_PATH);
    }
    else
    {
        throw(std::logic_error("Memory type is not defined"));
    }
}

bool create_link::isBootDeviceConfigured(const std::filesystem::path& config_path,
                                       const std::string& expected_boot_device)
{
    try
    {
        std::ifstream file(config_path, std::ios::in | std::ios::binary);
        if (!file.is_open())
        {
            return false;
        }

        std::string line;
        const std::string expected_device_path = "/dev/" + expected_boot_device;

        while (std::getline(file, line))
        {
            // Search for lines that contain device paths
            if (line.find("/dev/") != std::string::npos)
            {
                // Check if the line contains the expected boot device path
                if (line.find(expected_device_path) != std::string::npos)
                {
                    return true;
                }
            }
        }
        return false;
    }
    catch (...)
    {
        // If any error occurs (e.g., file not found, read error), assume the device is not configured
        return false;
    }
}
