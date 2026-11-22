#pragma once

#include <filesystem>
#include <string>

namespace mizuki {

struct SiteConfig;

struct NewContent {
    std::filesystem::path path;
    std::string title;
    std::string description;
    bool draft{false};
};

std::filesystem::path create_content(SiteConfig const& config, NewContent const& content);

} // namespace mizuki
