#pragma once

#include "config/site.hpp"

#include <cstddef>

namespace mizuki {

struct BuildStats {
    std::size_t pages_built{0};
    std::size_t assets_copied{0};
    bool published{true};
};

// Returns after publishing the complete site or confirming that it is unchanged.
BuildStats build_site(SiteConfig const& config);

} // namespace mizuki
