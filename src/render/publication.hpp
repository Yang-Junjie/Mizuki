#pragma once

#include "config/site.hpp"
#include "render/content.hpp"

#include <span>
#include <string>
#include <string_view>

namespace mizuki {

std::string render_rss(SiteMetadata const& site, BaseUrl const& base,
                       std::span<ContentSummary const> articles);
std::string render_sitemap(std::span<std::string const> urls);

} // namespace mizuki
