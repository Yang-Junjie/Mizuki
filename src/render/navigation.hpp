#pragma once

#include "config/site.hpp"

#include <span>
#include <string>

namespace mizuki {

std::string render_navigation(std::span<NavigationItem const> links);

} // namespace mizuki
