#include "parse/markdown.hpp"
#include "util/file.hpp"

#include <exception>
#include <utility>

namespace mizuki {

Markdown::Markdown(std::filesystem::path markdown_path, bool require_date)
    : markdown_path_(std::move(markdown_path)), require_date_(require_date) {}

bool Markdown::load() {
    loaded_ = false;
    html_.clear();
    front_matter_ = {};
    error_.clear();

    std::string source;
    try {
        source = read_file(markdown_path_);
    } catch (std::exception const& error) {
        error_ = error.what();
        return false;
    }

    std::string_view body;
    if (!parse_front_matter(source, front_matter_, body, error_, require_date_)) {
        return false;
    }

    body_ = std::string(body);
    loaded_ = true;
    return true;
}

bool Markdown::to_html() {
    if (!loaded_ && !load()) {
        html_.clear();
        return false;
    }

    std::string html;
    int const rc = md_html(body_.data(), static_cast<MD_SIZE>(body_.size()), on_html_output, &html,
                           MD_DIALECT_GITHUB, 0);
    if (rc != 0) {
        error_ = "Failed to convert Markdown";
        html_.clear();
        return false;
    }

    html_ = std::move(html);
    return true;
}

FrontMatter const& Markdown::front_matter() const {
    return front_matter_;
}

std::string const& Markdown::get_html() const {
    return html_;
}

std::string Markdown::get_html_or(std::string fallback) {
    return to_html() ? html_ : std::move(fallback);
}

std::string const& Markdown::error() const {
    return error_;
}

void Markdown::on_html_output(const MD_CHAR* text, MD_SIZE size, void* userdata) {
    static_cast<std::string*>(userdata)->append(text, size);
}

}
