#include "render/template.hpp"

#include "util/file.hpp"
#include "util/html.hpp"
#include "util/path.hpp"

#include <exception>
#include <stdexcept>
#include <utility>

namespace mizuki {

Template::Template(std::filesystem::path path) : path_(std::move(path)) {
    try {
        source_ = read_file(path_);
    } catch (std::exception const& error) {
        throw std::runtime_error("Failed to load template " + path_to_utf8(path_) + ": " +
                                 error.what());
    }
}

std::string Template::render(std::span<TemplateValue const> values) const {
    for (auto const& value : values) {
        if (value.placeholder.empty()) {
            throw std::runtime_error("Empty placeholder in template " + path_to_utf8(path_));
        }
        if (value.requirement == Requirement::Required &&
            source_.find(value.placeholder) == std::string::npos) {
            throw std::runtime_error("Missing required placeholder '" +
                                     std::string(value.placeholder) + "' in template " +
                                     path_to_utf8(path_));
        }
    }

    std::string page;
    std::size_t offset = 0;
    while (offset < source_.size()) {
        std::size_t next = std::string_view::npos;
        TemplateValue const* chosen = nullptr;
        for (auto const& value : values) {
            auto const position = source_.find(value.placeholder, offset);
            if (position < next) {
                next = position;
                chosen = &value;
            }
        }
        if (chosen == nullptr) {
            page.append(source_, offset, std::string::npos);
            break;
        }
        page.append(source_, offset, next - offset);
        if (chosen->kind == ValueKind::Html) {
            page.append(chosen->value);
        } else {
            page.append(escape_html(chosen->value));
        }
        offset = next + chosen->placeholder.size();
    }
    return page;
}

} // namespace mizuki
