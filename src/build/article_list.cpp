#include "build/article_list.hpp"

#include "util/html.hpp"

#include <algorithm>

namespace mizuki {

namespace {

bool is_unreserved(unsigned char ch) {
    return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
           (ch >= '0' && ch <= '9') || ch == '-' || ch == '.' || ch == '_' || ch == '~';
}

std::string encode_url_path(std::string const& path) {
    constexpr char hex[] = "0123456789ABCDEF";
    std::string encoded;
    encoded.reserve(path.size());
    for (unsigned char ch : path) {
        if (is_unreserved(ch) || ch == '/') {
            encoded += static_cast<char>(ch);
        } else {
            encoded += '%';
            encoded += hex[ch >> 4];
            encoded += hex[ch & 0x0F];
        }
    }
    return encoded;
}

}  // namespace

std::string render_article_list(std::vector<ArticleSummary> articles) {
    std::ranges::sort(articles, [](ArticleSummary const& left, ArticleSummary const& right) {
        if (left.date != right.date) {
            return left.date > right.date;
        }
        return left.url < right.url;
    });

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

}  // namespace mizuki
