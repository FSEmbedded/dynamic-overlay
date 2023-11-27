#include "dynamic_mounting.h"
#include "persistent_mem_detector.h"

#include <inicpp/inicpp.h>
#include <vector>
#include <cstdlib>
#include <algorithm>
#include <functional>
#include <filesystem>

#ifndef APP_IMAGE_DIR
#define APP_IMAGE_DIR "/rw_fs/root/application/"
#endif

#define ROLLBACK_APP_FW_REBOOT_PENDING 9
#define INCOMPLETE_APP_FW_ROLLBACK 12

DynamicMounting::DynamicMounting(const std::shared_ptr<UBoot> & uboot):
    overlay_workdir(DEFAULT_WORKDIR_PATH),
    overlay_upperdir(DEFAULT_UPPERDIR_PATH),
    appimage_currentdir(DEFAULT_APPLICATION_PATH),
    application_image_folder(std::regex("ApplicationFolder")),
    persistent_memory_image(std::regex("PersistentMemory\\..*")),
    uboot_handler(uboot)
{

}

void DynamicMounting::mount_application() const
{
    Mount mount = Mount();

    const char application = uboot_handler->getVariable("application", std::vector<char>({'A', 'B'}));
    std::filesystem::path app_mount_dir(APP_IMAGE_DIR);
    std::string application_image("app_a.squashfs");

    if (this->detect_failedUpdate_app_fw_reboot() == false)
    {
        if (application == 'B')
        {
            application_image = "app_b.squashfs";
        }
    }
    else
    {
        /* Detect reboot after failed update fails. This state is possible
         *  if update fails and boot order left is 0 or
         *  rollback is in progress.
         */

        /* Get update reboot state to check for rollback.
         *   9 : ROLLBACK_APP_FW_REBOOT_PENDING
         *       In case that reboot was executed before apply.
         *   12: INCOMPLETE_APP_FW_ROLLBACK
         *       Normal case, apply occurs.
         */
        const std::vector<uint8_t> allowed_update_reboot_state_variables({0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12});
        uint8_t update_reboot_state =
            uboot_handler->getVariable("update_reboot_state", allowed_update_reboot_state_variables);
        /* Update fails or rollback in progress */
        if ((application == 'B') && ((update_reboot_state == ROLLBACK_APP_FW_REBOOT_PENDING) ||
                                     (update_reboot_state == INCOMPLETE_APP_FW_ROLLBACK)))
        {
            /* in case of app_b and rollback in progress*/
            application_image = "app_b.squashfs";
        }
        else if ((application == 'A') && ((update_reboot_state != ROLLBACK_APP_FW_REBOOT_PENDING) &&
                                          (update_reboot_state != INCOMPLETE_APP_FW_ROLLBACK)))
        {
            application_image = "app_b.squashfs";
        } /* otherwise default app_a */
    }

    mount.mount_application_image((app_mount_dir / application_image.c_str()));
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
                    this->overlay_persistent[section.get_name()].lower_directory = std::string(entry.get<inicpp::string_ini_t>());
                }
                else if (entry.get_name() == "upperdir")
                {
                    this->overlay_persistent[section.get_name()].upper_directory = std::string(entry.get<inicpp::string_ini_t>());
                }
                else if (entry.get_name() == "workdir")
                {
                    this->overlay_persistent[section.get_name()].work_directory = std::string(entry.get<inicpp::string_ini_t>());
                }
                else if (entry.get_name() == "mergedir")
                {
                    this->overlay_persistent[section.get_name()].merge_directory = std::string(entry.get<inicpp::string_ini_t>());
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
                add_entry->lower_directory = add_entry->lower_directory + std::string(":") + add_entry->merge_directory;
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
                overlay_desc.merge_directory = std::string(entry);

                overlay_desc.lower_directory = std::string(entry);
                overlay_desc.lower_directory += std::string(std::string(":") + std::string(DEFAULT_APPLICATION_PATH));
                overlay_desc.lower_directory += std::string(entry);
                
                for (auto add_entry = this->additional_lower_directory_to_persistent.begin(); add_entry != this->additional_lower_directory_to_persistent.end(); add_entry++)
                {
                    if (add_entry->merge_directory == overlay_desc.merge_directory)
                    {
                        overlay_desc.lower_directory += std::string(std::string(":") + add_entry->lower_directory);
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


    const std::string boot_order = uboot_handler->getVariable("BOOT_ORDER", std::vector<std::string>({"A B", "B A"}));
    const std::string boot_order_old = uboot_handler->getVariable("BOOT_ORDER_OLD", std::vector<std::string>({"A B", "B A"}));
    const std::string rauc_cmd = uboot_handler->getVariable("rauc_cmd", std::vector<std::string>({"rauc.slot=A", "rauc.slot=B"}));
    const uint8_t number_of_tries_a = uboot_handler->getVariable("BOOT_A_LEFT", std::vector<uint8_t>({0, 1, 2, 3}));
    const uint8_t number_of_tries_b = uboot_handler->getVariable("BOOT_B_LEFT", std::vector<uint8_t>({0, 1, 2, 3}));

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
    /* Remove tmp.app in case the file tmp.app is available
     * This is the application update temporary file before rename to
     * updated application image. In case update fails old version should be
     * removed.
     */
    std::filesystem::path app_mount_dir(APP_IMAGE_DIR);
    std::filesystem::remove( app_mount_dir / "tmp.app");
}

void DynamicMounting::add_lower_dir_readonly_memory(const OverlayDescription::ReadOnly & container)
{
    this->additional_lower_directory_to_persistent.push_back(container);
}

DynamicMounting::~DynamicMounting()
{
}
