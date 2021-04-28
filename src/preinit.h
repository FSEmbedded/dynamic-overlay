#pragma once
#include <filesystem>
#include <vector>
#include <string>

#include "mount.h"
#include "dynamic_mounting.h"

namespace PreInit
{
    
    struct MountArgs{
        std::filesystem::path source_dir;
        std::filesystem::path dest_dir;
        std::string options;
        std::string filesystem_type;
        unsigned long flags;
        MountArgs()
        {   
            this->source_dir = std::filesystem::path("");
            this->dest_dir = std::filesystem::path("");
            this->options = "";
            this->filesystem_type = std::string("");
            this->flags = 0;
        }
        MountArgs(const MountArgs & cl)
        {
            this->source_dir        = cl.source_dir;
            this->dest_dir          = cl.dest_dir;
            this->options           = cl.options;
            this->filesystem_type   = cl.filesystem_type;
            this->flags             = cl.flags;
        }
    };

    class PreInit
    {
        private:
            static bool one_time_init;
            void handle_preaparation_error();
            std::vector<std::string> mounted_paths;
            std::vector<MountArgs> mount_prep;

        public:
            PreInit();
            ~PreInit();

            PreInit(const PreInit &) = delete;
            PreInit &operator=(const PreInit &) = delete;
            PreInit(PreInit &&) = delete;
            PreInit &operator=(PreInit &&) = delete;

            bool prepare();
            void add(const MountArgs &);
            void remove(const std::filesystem::path &) const;
    };
};