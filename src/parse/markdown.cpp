#include "parse/markdown.hpp"
#include "util/file.hpp"

#include <exception>
#include <utility>

namespace mizuki {

Markdown::Markdown(std::filesystem::path markdown_path)
    : markdown_path_(std::move(markdown_path)) {}

bool Markdown::to_html() {
    std::string markdown_content;
    try {
        markdown_content = read_file(markdown_path_);
    } catch (std::exception const&) {
        html_.clear();
        return false;
    }

    std::string html;
    int const rc = md_html(markdown_content.data(), static_cast<MD_SIZE>(markdown_content.size()),
                           on_html_output, &html, MD_DIALECT_GITHUB, 0);
    if (rc != 0) {
        html_.clear();
        return false;
    }

    html_ = std::move(html);
    return true;
}

std::string const& Markdown::get_html() const {
    return html_;
}

std::string Markdown::get_html_or(std::string fallback) {
    return to_html() ? html_ : std::move(fallback);
}

void Markdown::on_html_output(const MD_CHAR* text, MD_SIZE size, void* userdata) {
    static_cast<std::string*>(userdata)->append(text, size);
}

}
