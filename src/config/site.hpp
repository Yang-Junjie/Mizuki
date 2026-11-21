#pragma once

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

}  // namespace mizuki
