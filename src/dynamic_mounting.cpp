#include "dynamic_mounting.h"

bool DynamicMounting::application_mounted = false;


DynamicMounting::DynamicMounting():
    overlay_workdir(DEFAULT_WORKDIR_PATH),
    overlay_upperdir(DEFAULT_UPPERDIR_PATH),
    appimage_currentdir(DEFAULT_APPLICATION_PATH),
    application_image_folder(std::regex("ApplicationFolder")),
    persistent_memory_image(std::regex("PersistentMemory\\..*"))
{

}

void DynamicMounting::mount_application() const
{
    UBoot uboot = UBoot();
    Mount mount = Mount();

    const std::string application = uboot.getVariable("application");

    if (application == "A")
    {
        mount.mount_application_image("/rw_fs/root/application/app_a.squashfs");
    }
    else if (application == "B")
    {
        mount.mount_application_image("/rw_fs/root/application/app_b.squashfs");
    }
    else
    {
        throw(ApplicationImageNotCorrectDefined());
    }
}

void DynamicMounting::read_and_parse_ini()
{
    inicpp::config overlay_config = inicpp::parser::load_file(DEFAULT_OVERLAY_PATH);

    for (auto &section: overlay_config)
    {
        if (std::regex_match(section.get_name(), application_image_folder))
        {   
           
            for (auto &entry: section)
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
            for (auto &entry: section)
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

void DynamicMounting::mount_overlay() const{
    Mount mount = Mount();

    for (const std::string &entry: this->overlay_application)
    {
        OverlayDescription::ReadOnly overlay_desc = OverlayDescription::ReadOnly();
        overlay_desc.merge_directory = std::filesystem::path(entry);

        overlay_desc.lower_directory = std::filesystem::path(entry);
        overlay_desc.lower_directory += std::filesystem::path(std::string(":") + std::string(DEFAULT_APPLICATION_PATH));
        overlay_desc.lower_directory += std::filesystem::path(entry);

        mount.mount_overlay_readonly(overlay_desc);
    }

    for (auto &section: this->overlay_persistent)
    {
        mount.mount_overlay_persistent(section.second);
    }
}

void DynamicMounting::application_image()
{
    if (application_mounted == true)
    {
        throw(ApplicationImageAlreadyMounted());
    }
    else
    {
        application_mounted = true;
    }

    this->mount_application();
    this->read_and_parse_ini();
    this->mount_overlay();
}

DynamicMounting::~DynamicMounting()
{

}