#include "parse/markdown.hpp"

#include "parse/headings.hpp"
#include "util/file.hpp"
#include "util/path.hpp"

#include <md4c-html.h>

#include <exception>
#include <limits>
#include <stdexcept>

namespace mizuki {
namespace {

void append_html(MD_CHAR const* text, MD_SIZE size, void* output) {
    static_cast<std::string*>(output)->append(text, size);
}

} // namespace

Document read_document(std::filesystem::path const& path, DocumentKind kind) {
    try {
        return parse_document(read_file(path), kind);
    } catch (std::exception const& error) {
        throw std::runtime_error("Failed to parse " + path_to_utf8(path) + ": " + error.what());
    }
}

RenderedMarkdown render_markdown(std::string_view body) {
    if (body.size() > std::numeric_limits<MD_SIZE>::max()) {
        throw std::runtime_error("Markdown document exceeds parser size limit");
    }
    std::string html;
    auto const result = md_html(body.data(), static_cast<MD_SIZE>(body.size()), append_html, &html,
                                MD_DIALECT_GITHUB, 0);
    if (result != 0) {
        throw std::runtime_error("Failed to convert Markdown");
    }
    return add_heading_anchors(html);
}

} // namespace mizuki
