#include "render/publication.hpp"

#include "util/date.hpp"
#include "util/html.hpp"
#include "util/url.hpp"

#include <cstdint>
#include <stdexcept>

namespace mizuki {
namespace {

std::string escape_xml(std::string_view text) {
    auto invalid = [] {
        throw std::runtime_error("Invalid UTF-8 or XML character in publication metadata");
    };
    for (std::size_t i = 0; i < text.size();) {
        auto const first = static_cast<unsigned char>(text[i]);
        std::size_t length = 1;
        std::uint32_t codepoint = first;
        std::uint32_t minimum = 0;
        if (first >= 0xC2 && first <= 0xDF) {
            length = 2;
            codepoint = first & 0x1F;
            minimum = 0x80;
        } else if (first >= 0xE0 && first <= 0xEF) {
            length = 3;
            codepoint = first & 0x0F;
            minimum = 0x800;
        } else if (first >= 0xF0 && first <= 0xF4) {
            length = 4;
            codepoint = first & 0x07;
            minimum = 0x10000;
        } else if (first >= 0x80) {
            invalid();
        }
        if (i + length > text.size()) {
            invalid();
        }
        for (std::size_t j = 1; j < length; ++j) {
            auto const next = static_cast<unsigned char>(text[i + j]);
            if ((next & 0xC0) != 0x80) {
                invalid();
            }
            codepoint = (codepoint << 6) | (next & 0x3F);
        }
        bool const allowed = codepoint == 0x09 || codepoint == 0x0A || codepoint == 0x0D ||
                             (codepoint >= 0x20 && codepoint <= 0xD7FF) ||
                             (codepoint >= 0xE000 && codepoint <= 0xFFFD) ||
                             (codepoint >= 0x10000 && codepoint <= 0x10FFFF);
        if (codepoint < minimum || !allowed) {
            invalid();
        }
        i += length;
    }
    return escape_html(text);
}

} // namespace

std::string render_rss(SiteMetadata const& site, BaseUrl const& base,
                       std::span<ContentSummary const> articles) {
    auto const home = escape_xml(published_url(base, ""));
    auto const feed = escape_xml(published_url(base, "rss.xml"));
    std::string xml = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                      "<rss version=\"2.0\" xmlns:atom=\"http://www.w3.org/2005/Atom\">\n"
                      "  <channel>\n";
    xml += "    <title>" + escape_xml(site.title) + "</title>\n";
    xml += "    <link>" + home + "</link>\n";
    xml += "    <description>" +
           escape_xml(site.description.empty() ? site.title : site.description) +
           "</description>\n";
    xml += "    <language>" + escape_xml(site.language) + "</language>\n";
    xml += "    <atom:link href=\"" + feed + "\" rel=\"self\" type=\"application/rss+xml\"/>\n";
    for (auto const& article : articles) {
        auto const url = escape_xml(published_url(base, article.url));
        xml += "    <item>\n";
        xml += "      <title>" + escape_xml(article.title) + "</title>\n";
        xml += "      <link>" + url + "</link>\n";
        xml += "      <guid isPermaLink=\"true\">" + url + "</guid>\n";
        xml += "      <pubDate>" + rss_date(article.date) + "</pubDate>\n";
        if (!article.description.empty()) {
            // RSS item descriptions contain HTML; preserve this plain-text field literally.
            xml += "      <description>" + escape_xml(escape_html(article.description)) +
                   "</description>\n";
        }
        xml += "    </item>\n";
    }
    xml += "  </channel>\n</rss>\n";
    return xml;
}

std::string render_sitemap(std::span<std::string const> urls) {
    if (urls.size() > 50000) {
        throw std::runtime_error("Sitemap exceeds the single-file limit of 50000 URLs");
    }
    std::string xml = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                      "<urlset xmlns=\"http://www.sitemaps.org/schemas/sitemap/0.9\">\n";
    for (auto const& url : urls) {
        if (url.size() >= 2048) {
            throw std::runtime_error("Sitemap URL must be shorter than 2048 characters: " + url);
        }
        xml += "  <url><loc>" + escape_xml(url) + "</loc></url>\n";
    }
    xml += "</urlset>\n";
    if (xml.size() > 50 * 1024 * 1024) {
        throw std::runtime_error("Sitemap exceeds the single-file limit of 50 MiB");
    }
    return xml;
}

} // namespace mizuki
