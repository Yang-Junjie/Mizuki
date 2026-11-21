#include "render/navigation.hpp"

#include "util/html.hpp"

namespace mizuki {

std::string render_navigation(std::span<NavigationItem const> links) {
    if (links.empty()) {
        return {};
    }
    std::string html = "<nav aria-label=\"主导航\"><ul class=\"site-nav\">";
    for (auto const& item : links) {
        html += "<li><a href=\"" + escape_html(item.url) + "\">" + escape_html(item.title) +
                "</a></li>";
    }
    html += "</ul></nav>";
    return html;
}

} // namespace mizuki
