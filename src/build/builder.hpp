#pragma once

#include "config/site.hpp"

#include <chrono>
#include <cstddef>

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

} // namespace mizuki
