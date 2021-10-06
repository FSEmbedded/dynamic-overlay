#include "dynamic_mounting.h"
#include "persistent_mem_detector.h"

#include <inicpp/inicpp.h>
#include <vector>
#include <cstdlib>
#include <algorithm>
#include <functional>


DynamicMounting::DynamicMounting(const std::filesystem::path & fw_env): 
    overlay_workdir(DEFAULT_WORKDIR_PATH),
    overlay_upperdir(DEFAULT_UPPERDIR_PATH),
    appimage_currentdir(DEFAULT_APPLICATION_PATH),
    application_image_folder(std::regex("ApplicationFolder")),
    persistent_memory_image(std::regex("PersistentMemory\\..*")),
    uboot_env_config(fw_env)
{

}

void DynamicMounting::mount_application() const
{
    UBoot uboot = UBoot(this->uboot_env_config);
    Mount mount = Mount();

    const char application = uboot.getVariable("application", std::vector<char>({'A', 'B'}));

    if(this->detect_failedUpdate_app_fw_reboot() == false)
    {
        if(application == 'A')
        {
            mount.mount_application_image("/rw_fs/root/application/app_a.squashfs");
        }
        else
        {
            mount.mount_application_image("/rw_fs/root/application/app_b.squashfs");
        }
    }
    else
    {
        // Means that after a failed reboot during combined firmware and application update is not resetted.
        // This solves this piece of code.
        if(application == 'B')
        {
            mount.mount_application_image("/rw_fs/root/application/app_a.squashfs");
        }
        else
        {
            mount.mount_application_image("/rw_fs/root/application/app_b.squashfs");
        }
    }

}

void DynamicMounting::read_and_parse_ini()
{
    inicpp::config overlay_config = inicpp::parser::load_file(DEFAULT_OVERLAY_PATH);

    for (auto &section : overlay_config)
    {
        if (std::regex_match(section.get_name(), application_image_folder))
        {

            for (auto &entry : section)
            {
                if (!entry.is_list())
                {
                    this->overlay_application.push_back(entry.get<inicpp::string_ini_t>());
                }
                else
                {
                    throw(ININotDefinedSection("List-Element"));
                }
            }
        }
        else if (std::regex_match(section.get_name(), persistent_memory_image))
        {
            this->overlay_persistent[section.get_name()] = OverlayDescription::Persistent();
            for (auto &entry : section)
            {
                if (entry.get_name() == "lowerdir")
                {
                    this->overlay_persistent[section.get_name()].lower_directory = std::filesystem::path(entry.get<inicpp::string_ini_t>());
                }
                else if (entry.get_name() == "upperdir")
                {
                    this->overlay_persistent[section.get_name()].upper_directory = std::filesystem::path(entry.get<inicpp::string_ini_t>());
                }
                else if (entry.get_name() == "workdir")
                {
                    this->overlay_persistent[section.get_name()].work_directory = std::filesystem::path(entry.get<inicpp::string_ini_t>());
                }
                else if (entry.get_name() == "mergedir")
                {
                    this->overlay_persistent[section.get_name()].merge_directory = std::filesystem::path(entry.get<inicpp::string_ini_t>());
                }
                else
                {
                    throw(ININotDefinedEntry(section.get_name(), entry.get<inicpp::string_ini_t>()));
                }
            }
        }
        else
        {
            throw(ININotDefinedSection(section.get_name()));
        }
    }
}

void DynamicMounting::mount_overlay_read_only(bool application_mounted_overlay_parsed)
{
    Mount mount;

    auto mount_ramdisk = [&](){
        for (auto add_entry = this->additional_lower_directory_to_persistent.begin(); add_entry != this->additional_lower_directory_to_persistent.end(); add_entry++)
        {
            if(std::find(used_entries_application_overlay.begin(), used_entries_application_overlay.end(), add_entry) == used_entries_application_overlay.end())
            {
                add_entry->lower_directory = add_entry->lower_directory.string() + std::string(":") + add_entry->merge_directory.string();
                mount.mount_overlay_readonly(*add_entry);
            }
        }

        this->additional_lower_directory_to_persistent.clear();
        this->used_entries_application_overlay.clear();
    };

    if (application_mounted_overlay_parsed)
    {
        try
        {      
            for (const std::string &entry : this->overlay_application)
            {
                OverlayDescription::ReadOnly overlay_desc = OverlayDescription::ReadOnly();
                overlay_desc.merge_directory = std::filesystem::path(entry);

                overlay_desc.lower_directory = std::filesystem::path(entry);
                overlay_desc.lower_directory += std::filesystem::path(std::string(":") + std::string(DEFAULT_APPLICATION_PATH));
                overlay_desc.lower_directory += std::filesystem::path(entry);
                
                for (auto add_entry = this->additional_lower_directory_to_persistent.begin(); add_entry != this->additional_lower_directory_to_persistent.end(); add_entry++)
                {
                    if (add_entry->merge_directory == overlay_desc.merge_directory)
                    {
                        overlay_desc.lower_directory += std::filesystem::path(std::string(":") + add_entry->lower_directory.string());
                        used_entries_application_overlay.push_back(add_entry);
                    }
                }

                mount.mount_overlay_readonly(overlay_desc);
            }
        }
        catch(...)
        {
            // Ramdisk is always mounted
            mount_ramdisk();
            throw;
        }
    }

    mount_ramdisk();
}

void DynamicMounting::mount_overlay_persistent()
{
    Mount mount;

    for (auto &section : this->overlay_persistent)
    {
        mount.mount_overlay_persistent(section.second);
    }
}

bool DynamicMounting::detect_failedUpdate_app_fw_reboot() const
{

    std::function<std::vector<std::string>(const std::string &, const char)> split = [](const std::string &input, const char split) -> std::vector<std::string>
    {
        std::vector<std::string> return_element;
        std::string output;
        for (const char elem: input)
        {
            if (elem != split)
            {
                output.append(&elem,1);
            }
            else
            {
                return_element.push_back(output);
                output.clear();
            }
        }
        return_element.push_back(output);
        output.clear();
        return return_element;
    };

    UBoot uboot = UBoot(this->uboot_env_config);
    const std::string boot_order = uboot.getVariable("BOOT_ORDER", std::vector<std::string>({"A B", "B A"}));
    const std::string boot_order_old = uboot.getVariable("BOOT_ORDER_OLD", std::vector<std::string>({"A B", "B A"}));
    const std::string rauc_cmd = uboot.getVariable("rauc_cmd", std::vector<std::string>({"rauc.slot=A", "rauc.slot=B"}));
    const uint8_t number_of_tries_a = uboot.getVariable("BOOT_A_LEFT", std::vector<uint8_t>({0, 1, 2, 3}));
    const uint8_t number_of_tries_b = uboot.getVariable("BOOT_B_LEFT", std::vector<uint8_t>({0, 1, 2, 3}));

    const std::string current_slot = split(rauc_cmd, '=').back();

    const bool firmware_update_reboot_failed = ((current_slot == split(boot_order_old, ' ').front()) && ((number_of_tries_a == 0) || (number_of_tries_b == 0))) && (boot_order_old != boot_order);
    
    return firmware_update_reboot_failed;
}

void DynamicMounting::application_image()
{
    try
    {
        this->mount_application();
        this->read_and_parse_ini();
    }
    catch(...)
    {
        this->mount_overlay_read_only(false);
        throw;
    }
    this->mount_overlay_read_only(true);
    this->mount_overlay_persistent();
}

void DynamicMounting::add_lower_dir_readonly_memory(const OverlayDescription::ReadOnly & container)
{
    this->additional_lower_directory_to_persistent.push_back(container);
}

DynamicMounting::~DynamicMounting()
{

}
