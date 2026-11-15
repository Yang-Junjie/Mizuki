#include "render/renderer.hpp"
#include "util/file.hpp"

#include <algorithm>
#include <exception>
#include <utility>

namespace mizuki {

Renderer::Renderer(std::filesystem::path layout_path) : layout_path_(std::move(layout_path)) {}

bool Renderer::render(std::string_view content, std::string_view asset_base) {
    std::string layout;
    try {
        layout = read_file(layout_path_);
    } catch (std::exception const&) {
        page_.clear();
        return false;
    }

    constexpr std::string_view content_placeholder = "{markdown}";
    constexpr std::string_view assets_placeholder = "{asset_base}";
    if (layout.find(content_placeholder) == std::string::npos) {
        page_.clear();
        return false;
    }

    std::string page;
    std::size_t offset = 0;
    while (offset < layout.size()) {
        auto const content_pos = layout.find(content_placeholder, offset);
        auto const assets_pos = layout.find(assets_placeholder, offset);
        auto const next = std::min(content_pos, assets_pos);
        if (next == std::string::npos) {
            page.append(layout, offset, std::string::npos);
            break;
        }
        page.append(layout, offset, next - offset);
        if (next == content_pos) {
            page.append(content);
            offset = next + content_placeholder.size();
        } else {
            page.append(asset_base);
            offset = next + assets_placeholder.size();
        }
    }
    page_ = std::move(page);
    return true;
}

std::string const& Renderer::get_html() const {
    return page_;
}

}  // namespace mizuki
