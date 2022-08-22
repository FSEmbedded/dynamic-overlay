#include "create_link.h"
#include <stdexcept>

/**
 * Creates a soft link to the system_conf file depending on the board memory type.
 * This file is used by RAUC.
 */
void create_link_to_system_conf(const PersistentMemDetector::MemType &);

/**
 * Creates a soft link to the fw_env_conf file depending on the board memory type.
 * This file is used by system tools like fw_printenv.
 */
void create_link_to_fw_env_conf(const PersistentMemDetector::MemType &);

void create_link_to_system_conf(const PersistentMemDetector::MemType &type)
{
    std::filesystem::path source, destination;

    if (type == PersistentMemDetector::MemType::NAND)
    {
        source = std::filesystem::path(NAND_RAUC_SYSTEM_CONF_PATH);
        destination = std::filesystem::path(RAUC_SYSTEM_CONF_PATH);
    }
    else
    {
        source = std::filesystem::path(EMMC_RAUC_SYSTEM_CONF_PATH);
        destination = std::filesystem::path(RAUC_SYSTEM_CONF_PATH);
    }

    try
    {
        std::filesystem::create_symlink(source, destination);
    }
    catch (...)
    {
        create_link::CreateSymlink(source, destination);
    }
}

void create_link_to_fw_env_conf(const PersistentMemDetector::MemType &type)
{
    std::filesystem::path source, destination;

    if (type == PersistentMemDetector::MemType::NAND)
    {
        source = std::filesystem::path(NAND_UBOOT_ENV_PATH);
        destination = std::filesystem::path(UBOOT_ENV_PATH);
    }
    else
    {
        source = std::filesystem::path(EMMC_UBOOT_ENV_PATH);
        destination = std::filesystem::path(UBOOT_ENV_PATH);
    }

    try
    {
        std::filesystem::create_symlink(source, destination);
    }
    catch (...)
    {
        create_link::CreateSymlink(source, destination);
    }
}

OverlayDescription::ReadOnly create_link::prepare_ramdisk(const std::filesystem::path & path_to_ramdisk, const PersistentMemDetector::MemType &mem_type)
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
        "",
        "ramfs",
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

    create_link_to_system_conf(mem_type);
    create_link_to_fw_env_conf(mem_type);

    mount.wrapper_c_umount("/etc");
    mount.wrapper_c_mount(
        "none",
        path_to_ramdisk,
        "",
        "ramfs",
        MS_REMOUNT|MS_RDONLY
    );

    OverlayDescription::ReadOnly ramdisk_ro;
    ramdisk_ro.lower_directory = path_to_ramdisk;
    ramdisk_ro.lower_directory += "/upper/etc";

    ramdisk_ro.merge_directory = "/etc";

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
