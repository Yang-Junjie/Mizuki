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

RenderedMarkdown render_markdown(std::string_view body, MarkdownOptions const& options) {
    if (body.size() > std::numeric_limits<MD_SIZE>::max()) {
        throw std::runtime_error("Markdown document exceeds parser size limit");
    }
    unsigned flags = MD_DIALECT_GITHUB;
    if (options.math) {
        flags |= MD_FLAG_LATEXMATHSPANS;
    }
    if (!options.footnotes) {
        flags &= ~MD_FLAG_FOOTNOTES;
    }
    if (!options.admonitions) {
        flags &= ~MD_FLAG_ADMONITIONS;
    }
    std::string html;
    auto const result =
        md_html(body.data(), static_cast<MD_SIZE>(body.size()), append_html, &html, flags, 0);
    if (result != 0) {
        throw std::runtime_error("Failed to convert Markdown");
    }
    return analyze_markdown_html(html);
}

} // namespace mizuki
