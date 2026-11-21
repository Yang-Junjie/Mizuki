#pragma once

#include <string>
#include <vector>

namespace mizuki {

struct ArticleSummary {
    std::string title;
    std::string date;
    std::string url;
};

std::string render_article_list(std::vector<ArticleSummary> articles);

} // namespace mizuki
