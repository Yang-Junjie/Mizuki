#include "render/renderer.hpp"
#include "util/file.hpp"
#include "util/html.hpp"

#include <exception>
#include <utility>

namespace mizuki {

Renderer::Renderer(std::filesystem::path layout_path) : layout_path_(std::move(layout_path)) {}

bool Renderer::render(std::span<TemplateValue const> values) {
    std::string layout;
    try {
        layout = read_file(layout_path_);
    } catch (std::exception const&) {
        page_.clear();
        return false;
    }

    for (auto const& value : values) {
        if (value.placeholder.empty() || layout.find(value.placeholder) == std::string::npos) {
            page_.clear();
            return false;
        }
    }

    std::string page;
    std::size_t offset = 0;
    while (offset < layout.size()) {
        std::size_t next = std::string_view::npos;
        TemplateValue const* chosen = nullptr;
        for (auto const& value : values) {
            auto const position = layout.find(value.placeholder, offset);
            if (position < next) {
                next = position;
                chosen = &value;
            }
        }
        if (chosen == nullptr) {
            page.append(layout, offset, std::string::npos);
            break;
        }
        page.append(layout, offset, next - offset);
        if (chosen->raw_html) {
            page.append(chosen->value);
        } else {
            page.append(escape_html(chosen->value));
        }
        offset = next + chosen->placeholder.size();
    }
    page_ = std::move(page);
    return true;
}

std::string const& Renderer::get_html() const {
    return page_;
}

}  // namespace mizuki
