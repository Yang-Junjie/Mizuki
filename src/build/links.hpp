#pragma once

#include "build/plan.hpp"

#include <filesystem>
#include <string>
#include <string_view>

namespace mizuki {

std::string rewrite_document_links(std::string_view html, std::filesystem::path const& source,
                                   BuildPlan const& plan);
std::string render_navigation(SiteMetadata const& site, std::filesystem::path const& page,
                              BuildPlan const& plan);

}  // namespace mizuki
