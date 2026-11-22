#pragma once

#include <filesystem>
#include <span>
#include <string>

namespace mizuki {

struct ArticleSummary {
    std::string title;
    std::string date;
    std::string url;
    std::string description;
};

// Articles arrive in publication order, shared by listings, archives and RSS.
std::string render_article_list(std::span<ArticleSummary const> articles,
                                std::filesystem::path const& output);
std::string render_archive(std::span<ArticleSummary const> articles,
                           std::filesystem::path const& output);

} // namespace mizuki
