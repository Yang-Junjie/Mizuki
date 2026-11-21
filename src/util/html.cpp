#include "util/html.hpp"

namespace mizuki {

std::string escape_html(std::string_view text) {
    std::string escaped;
    escaped.reserve(text.size());
    for (char const ch : text) {
        switch (ch) {
        case '&':
            escaped += "&amp;";
            break;
        case '<':
            escaped += "&lt;";
            break;
        case '>':
            escaped += "&gt;";
            break;
        case '"':
            escaped += "&quot;";
            break;
        case '\'':
            escaped += "&#39;";
            break;
        default:
            escaped += ch;
            break;
        }
    }
    return escaped;
}

} // namespace mizuki
