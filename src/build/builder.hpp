#pragma once

#include "config/site.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <map>
#include <memory>
#include <set>
#include <string>

namespace mizuki {

struct BuildTimings {
    using Duration = std::chrono::duration<double, std::milli>;

    Duration planning{};
    Duration pages{};
    Duration assets{};
    Duration comparison{};
    Duration publishing{};
    Duration total{};
};

struct BuildStats {
    std::size_t pages_built{0};
    std::size_t assets_copied{0};
    bool published{true};
    BuildTimings timings;
};

// Returns after publishing the complete site or confirming that it is unchanged.
BuildStats build_site(SiteConfig const& config);

struct PreviewAsset {
    std::filesystem::path source;
    std::filesystem::path cached;
    std::filesystem::file_time_type modified;
    std::uintmax_t size{0};
    ~PreviewAsset();
};

struct PreviewFiles {
    std::map<std::string, std::shared_ptr<std::string const>, std::less<>> generated;
    std::map<std::string, std::shared_ptr<PreviewAsset const>, std::less<>> assets;
    std::set<std::string, std::less<>> directories;
};

struct PreviewBuild {
    std::shared_ptr<PreviewFiles const> files;
    BuildStats stats;
};

PreviewBuild build_preview(SiteConfig const& config, std::filesystem::path const& storage,
                           std::shared_ptr<PreviewFiles const> const& previous,
                           std::set<std::filesystem::path> const& changed);

} // namespace mizuki
