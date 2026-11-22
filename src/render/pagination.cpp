#include "render/pagination.hpp"

#include "util/html.hpp"

namespace mizuki {

std::string render_pagination(std::size_t current, std::size_t total, std::string_view previous,
                              std::string_view next) {
    if (total <= 1) {
        return {};
    }
    std::string html = "<nav class=\"pagination\" aria-label=\"文章分页\">";
    if (!previous.empty()) {
        html += "<a rel=\"prev\" href=\"" + escape_html(previous) + "\">上一页</a>";
    }
    html += "<span aria-current=\"page\">第 " + std::to_string(current) + " / " +
            std::to_string(total) + " 页</span>";
    if (!next.empty()) {
        html += "<a rel=\"next\" href=\"" + escape_html(next) + "\">下一页</a>";
    }
    html += "</nav>";
    return html;
}

} // namespace mizuki
