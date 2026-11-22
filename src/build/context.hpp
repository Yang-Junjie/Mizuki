#pragma once

#include "build/plan.hpp"
#include "parse/headings.hpp"

#include <filesystem>
#include <nlohmann/json.hpp>
#include <span>

namespace mizuki {

nlohmann::json page_context(SiteConfig const& config, BuildPlan const& plan,
                            std::filesystem::path const& output, nlohmann::json const& extra);
nlohmann::json article_data(std::span<ArticleSummary const> articles,
                            std::filesystem::path const& output);
nlohmann::json heading_data(std::span<Heading const> headings);
nlohmann::json toc_data(std::span<Heading const> headings);
std::string listing_url(std::filesystem::path const& target, std::filesystem::path const& source);

} // namespace mizuki
