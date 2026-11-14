#include "render/renderer.hpp"

#include <fstream>
#include <sstream>
#include <stdexcept>
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

std::string Renderer::read_file(std::filesystem::path const& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open file: " + path.string());
    }
    std::ostringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

}
