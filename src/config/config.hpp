#pragma once

#include "config/site.hpp"

#include <filesystem>
#include <optional>
#include <string>

namespace mizuki {

// CLI overrides stay separate so reloading a file never loses their precedence.
struct ConfigRequest {
    std::filesystem::path config_file{"mizuki.toml"};
    bool explicit_config{false};
    std::optional<std::filesystem::path> content_dir;
    std::optional<std::filesystem::path> theme_dir;
    std::optional<std::filesystem::path> output_dir;
    std::optional<std::string> base_url;
};

SiteConfig load_config(ConfigRequest const& request);

} // namespace mizuki
