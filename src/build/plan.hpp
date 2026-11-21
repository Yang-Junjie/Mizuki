#pragma once

#include "build/builder.hpp"
#include "parse/markdown.hpp"

#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace mizuki {

struct PlannedPage {
    std::filesystem::path source;
    std::filesystem::path relative;
    std::filesystem::path output;
    Markdown markdown;
    bool independent;
};

struct PlannedAsset {
    std::filesystem::path source;
    std::filesystem::path output;
};

struct BuildPlan {
    std::vector<PlannedPage> pages;
    std::vector<PlannedAsset> assets;
    std::map<std::filesystem::path, std::filesystem::path> page_urls;
    std::map<std::filesystem::path, std::string> outputs;
};

BuildPlan plan_build(BuildOptions const& options);

}  // namespace mizuki
