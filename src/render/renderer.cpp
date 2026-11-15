#include "render/renderer.hpp"
#include "util/file.hpp"

#include <exception>
#include <utility>

namespace mizuki {

Renderer::Renderer(std::filesystem::path layout_path)
    : layout_path_(std::move(layout_path)) {}

bool Renderer::render(std::string_view content) {
    std::string layout;
    try {
        layout = read_file(layout_path_);
    } catch (std::exception const&) {
        page_.clear();
        return false;
    }

    constexpr std::string_view placeholder = "{markdown}";
    std::size_t const pos = layout.find(placeholder);
    if (pos == std::string::npos) {
        page_.clear();
        return false;
    }

    layout.replace(pos, placeholder.size(), content);
    page_ = std::move(layout);
    return true;
}

std::string const& Renderer::get_html() const {
    return page_;
}

}
