#pragma once

#include "config/site.hpp"

#include <cstddef>

namespace mizuki {

struct BuildStats {
    std::size_t pages_built{0};
    std::size_t assets_copied{0};
};

// Returns only after the complete site has been published; failures throw.
BuildStats build_site(SiteConfig const& config);

} // namespace mizuki
