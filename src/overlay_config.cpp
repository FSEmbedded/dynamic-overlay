#include "overlay_config.h"
#include "logging.h"

#include <string>

namespace overlay_config {

Error parse_persistent_section(std::string_view section_name,
                                const ini::Section &section,
                                OverlayDescription::Persistent &out) noexcept
{
    const std::string section_str(section_name);

    for (const auto &[key, value] : section) {
        if (key == "lowerdir") {
            out.lower_directory = value;
        } else if (key == "upperdir") {
            out.upper_directory = value;
        } else if (key == "workdir") {
            out.work_directory = value;
        } else if (key == "mergedir") {
            out.merge_directory = value;
        } else if (key == "nosuid") {
            if (value == "true") {
                out.nosuid = true;
            } else if (value == "false") {
                out.nosuid = false;
            } else {
                LOG_ERROR("invalid nosuid value in " + section_str + ": " + value);
                return Error::config_invalid;
            }
        } else {
            LOG_WARNING("unknown entry in section " + section_str + ": " + key);
            return Error::config_invalid;
        }
    }

    if (out.lower_directory.empty() || out.upper_directory.empty() ||
        out.work_directory.empty() || out.merge_directory.empty()) {
        LOG_ERROR("missing required field in section " + section_str);
        return Error::config_invalid;
    }

    return Error::none;
}

} // namespace overlay_config
