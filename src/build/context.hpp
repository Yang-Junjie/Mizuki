#pragma once

#include "build/plan.hpp"
#include "parse/headings.hpp"

#include <filesystem>
#include <nlohmann/json.hpp>
#include <span>
#include <string_view>

namespace mizuki {

nlohmann::json page_context(SiteConfig const& config, BuildPlan const& plan,
                            std::filesystem::path const& output, nlohmann::json const& extra);
nlohmann::json summary_value(ContentSummary const& summary, std::filesystem::path const& output);
nlohmann::json term_links(std::span<std::string const> names, std::string_view taxonomy,
                          std::filesystem::path const& output);
nlohmann::json term_data(PlannedTaxonomy const& taxonomy, std::filesystem::path const& output);
nlohmann::json pagination_data(std::span<PlannedListing const> listings, std::size_t index);
nlohmann::json summary_data(std::span<ContentSummary const> summaries,
                            std::filesystem::path const& output);
nlohmann::json heading_data(std::span<Heading const> headings);
nlohmann::json toc_data(std::span<Heading const> headings);
std::string listing_url(std::filesystem::path const& target, std::filesystem::path const& source);

} // namespace mizuki
