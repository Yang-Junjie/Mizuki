#pragma once

#include "util/url.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace mizuki {

struct NavigationItem {
    std::string title;
    std::string url;
};

struct SiteMetadata {
    std::string title{"Mizuki"};
    std::string description{"一个用 C++ 编写的轻量博客框架"};
    std::string author;
    std::string language{"zh-CN"};
    std::vector<NavigationItem> navigation{{"首页", "/"}};
};

struct SiteConfig {
    std::filesystem::path content_dir{"content"};
    std::filesystem::path theme_dir{"themes/default"};
    std::filesystem::path output_dir{"dist"};
    std::filesystem::path config_file;
    SiteMetadata site;
    BaseUrl base_url;
};

} // namespace mizuki
