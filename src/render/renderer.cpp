#include "render/renderer.hpp"
#include "util/file.hpp"

#include <algorithm>
#include <array>
#include <exception>
#include <utility>

namespace mizuki {

namespace {

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

}  // namespace

Renderer::Renderer(std::filesystem::path layout_path) : layout_path_(std::move(layout_path)) {}

bool Renderer::render(std::string_view content, RenderValues const& values) {
    std::string layout;
    try {
        layout = read_file(layout_path_);
    } catch (std::exception const&) {
        page_.clear();
        return false;
    }

    auto const title = escape_html(values.title);
    auto const date = escape_html(values.date);
    std::array<std::pair<std::string_view, std::string_view>, 4> const placeholders{{
        {"{markdown}", content},
        {"{asset_base}", values.asset_base},
        {"{title}", title},
        {"{date}", date},
    }};

    for (auto const& [token, value] : placeholders) {
        if (layout.find(token) == std::string::npos) {
            page_.clear();
            return false;
        }
    }

    std::string page;
    std::size_t offset = 0;
    while (offset < layout.size()) {
        std::size_t next = std::string_view::npos;
        std::pair<std::string_view, std::string_view> const* chosen = nullptr;
        for (auto const& placeholder : placeholders) {
            auto const position = layout.find(placeholder.first, offset);
            if (position < next) {
                next = position;
                chosen = &placeholder;
            }
        }
        if (chosen == nullptr) {
            page.append(layout, offset, std::string::npos);
            break;
        }
        page.append(layout, offset, next - offset);
        page.append(chosen->second);
        offset = next + chosen->first.size();
    }
    page_ = std::move(page);
    return true;
}

std::string const& Renderer::get_html() const {
    return page_;
}

}  // namespace mizuki
