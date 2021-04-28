#include "preinit.h"

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

bool PreInit::PreInit::prepare()
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
        return false;
    }
    return true;
}

void PreInit::PreInit::remove(const std::filesystem::path &path) const
{
    Mount mount_handler = Mount();
    mount_handler.wrapper_c_umount(path);
}