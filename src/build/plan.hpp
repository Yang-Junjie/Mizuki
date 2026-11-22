#pragma once

#include "config/site.hpp"
#include "parse/front_matter.hpp"
#include "render/article.hpp"

#include <cstddef>
#include <filesystem>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace mizuki {

struct PlannedPage {
    std::filesystem::path source;
    std::filesystem::path relative;
    std::filesystem::path output;
    Document document;
    DocumentKind kind;
};

struct PlannedAsset {
    std::filesystem::path source;
    std::filesystem::path output;
};

struct PlannedListing {
    std::filesystem::path output;
    std::size_t offset;
    std::size_t count;
};

struct BuildPlan {
    std::string base_path{"/"};
    std::vector<PlannedPage> pages;
    std::vector<PlannedAsset> assets;
    std::vector<ArticleSummary> articles;
    std::vector<PlannedListing> listings;
    std::map<std::filesystem::path, std::filesystem::path> page_urls;
    std::set<std::filesystem::path> outputs;
};

BuildPlan plan_build(SiteConfig const& options);

} // namespace mizuki
