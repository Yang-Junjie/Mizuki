#include "render/article_list.hpp"

#include "util/html.hpp"
#include "util/path.hpp"
#include "util/url.hpp"

namespace mizuki {

std::string render_article_list(std::span<ArticleSummary const> articles,
                                std::filesystem::path const& output) {
    std::string html;
    for (auto const& article : articles) {
        html += "<li class=\"post-item\"><a href=\"";
        html += escape_html(relative_url(path_from_utf8(article.url), output));
        html += "\">";
        html += escape_html(article.title);
        html += "</a><time>";
        html += escape_html(article.date);
        html += "</time></li>\n";
    }
    return html;
}

std::string render_archive(std::span<ArticleSummary const> articles,
                           std::filesystem::path const& output) {
    if (articles.empty()) {
        return "<p class=\"empty-state\">暂无文章。</p>\n";
    }
    std::string html;
    while (!articles.empty()) {
        auto const year = articles.front().date.substr(0, 4);
        std::size_t count = 0;
        while (count < articles.size() && articles[count].date.starts_with(year)) {
            ++count;
        }
        html += "<section class=\"archive-year\" aria-labelledby=\"year-" + year + "\">\n";
        html += "<h2 id=\"year-" + year + "\">" + year + "</h2>\n<ul class=\"post-list\">\n";
        html += render_article_list(articles.first(count), output);
        html += "</ul>\n</section>\n";
        articles = articles.subspan(count);
    }
    return html;
}

} // namespace mizuki
