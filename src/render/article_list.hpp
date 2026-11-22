#pragma once

#include <span>
#include <string>

namespace mizuki {

struct ArticleSummary {
    std::string title;
    std::string date;
    std::string url;
    std::string description;
};

// Articles arrive in publication order, shared by the homepage and RSS.
std::string render_article_list(std::span<ArticleSummary const> articles);

} // namespace mizuki
