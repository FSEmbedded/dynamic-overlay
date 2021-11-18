#include "preinit.h"
#include <optional>
#include <algorithm>

PreInit::PreInit::PreInit()
{

}

PreInit::PreInit::~PreInit()
{

}

void PreInit::PreInit::add(const MountArgs &handler)
{
    this->mount_prep.push_back(handler);
}

void PreInit::PreInit::prepare()
{   
    Mount mount_handler = Mount();
    try 
    {  
        for (auto &entry: this->mount_prep)
        {   
            mount_handler.wrapper_c_mount(
                entry.source_dir,
                entry.dest_dir,
                entry.options,
                entry.filesystem_type,
                entry.flags
            );
            this->mounted_paths.push_back(entry.dest_dir);
        }
    }
    catch(const std::exception& e)
    {   
        for (auto &entry: this->mounted_paths)
        {
            mount_handler.wrapper_c_umount(entry);
        }
        throw;
    }
}

void PreInit::PreInit::remove(const MountArgs &obj)
{
    Mount mount_handler = Mount();
    std::optional<std::filesystem::path> path;
    for(const auto & entry: this->mounted_paths)
    {
        if(entry == obj.dest_dir)
        {
            path = std::optional<std::filesystem::path>(obj.dest_dir);
            break;
        }
    }
    if(path)
    {
        this->mounted_paths.erase(std::remove(this->mounted_paths.begin(), this->mounted_paths.end(), path.value()), this->mounted_paths.end());
        mount_handler.wrapper_c_umount(path.value());
    }
    else
    {
        std::string error_msg = "Mount object does not contain an already mounted destination path: ";
        error_msg += obj.dest_dir;
        throw(std::logic_error(error_msg));
    }
}