#include "render/article_list.hpp"

#include "util/html.hpp"
#include "util/url.hpp"

namespace mizuki {

std::string render_article_list(std::span<ArticleSummary const> articles) {
    std::string html;
    for (auto const& article : articles) {
        html += "<li class=\"post-item\"><a href=\"";
        html += escape_html(encode_url_path(article.url));
        html += "\">";
        html += escape_html(article.title);
        html += "</a><time>";
        html += escape_html(article.date);
        html += "</time></li>\n";
    }
    return html;
}

} // namespace mizuki
