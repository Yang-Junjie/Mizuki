#pragma once

#include "config/images.hpp"
#include "config/markdown.hpp"
#include "util/url.hpp"

#include <cstddef>
#include <filesystem>
#include <nlohmann/json.hpp>
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
    nlohmann::json extra = nlohmann::json::object();
    std::vector<NavigationItem> navigation{{"首页", "/"}};
};

struct SiteConfig {
    std::filesystem::path content_dir{"content"};
    std::filesystem::path theme_dir{"themes/default"};
    std::filesystem::path output_dir{"dist"};
    std::filesystem::path config_file;
    SiteMetadata site;
    MarkdownOptions markdown;
    ImageOptions images;
    bool incremental{false};
    BaseUrl base_url;
    std::size_t paginate_by{0};
    bool archives{false};
};

} // namespace mizuki
