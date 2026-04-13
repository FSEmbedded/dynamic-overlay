#include "ini_parser.h"
#include "posix_utils.h"
#include "string_utils.h"
#include "logging.h"

#include <string>
#include <string_view>

using string_utils::trim;

Error ini::parse_string(std::string_view content, Config &out) noexcept
{
    out.clear();
    std::string current_section;

    std::string_view remaining = content;
    while (!remaining.empty()) {
        // Extract one line
        std::string_view line;
        const auto nl = remaining.find('\n');
        if (nl != std::string_view::npos) {
            line = remaining.substr(0, nl);
            remaining.remove_prefix(nl + 1);
        } else {
            line = remaining;
            remaining = {};
        }

        line = trim(line);

        // Skip empty lines and comments
        if (line.empty() || line.front() == '#' || line.front() == ';') {
            continue;
        }

        // Section header
        if (line.front() == '[') {
            const auto close = line.find(']');
            if (close == std::string_view::npos) {
                LOG_ERROR("ini_parser: malformed section header");
                return Error::config_parse_failed;
            }
            current_section = std::string(trim(line.substr(1, close - 1)));
            if (current_section.empty()) {
                LOG_ERROR("ini_parser: empty section name");
                return Error::config_parse_failed;
            }
            out[current_section]; // ensure section exists
            continue;
        }

        // Key=value pair
        const auto eq = line.find('=');
        if (eq == std::string_view::npos) {
            LOG_ERROR("ini_parser: line without '=' outside section");
            return Error::config_parse_failed;
        }

        if (current_section.empty()) {
            LOG_ERROR("ini_parser: key=value before any section");
            return Error::config_parse_failed;
        }

        const auto key = trim(line.substr(0, eq));
        const auto val = trim(line.substr(eq + 1));

        if (key.empty()) {
            LOG_ERROR("ini_parser: empty key");
            return Error::config_parse_failed;
        }

        out[current_section][std::string(key)] = std::string(val);
    }

    return Error::none;
}

Error ini::parse_file(std::string_view path, Config &out) noexcept
{
    std::string content;
    const Error err = posix_utils::read_file_to_string(path, content);
    if (err != Error::none) {
        return err;
    }

    return parse_string(content, out);
}
