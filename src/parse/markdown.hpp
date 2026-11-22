#pragma once

#include "config/markdown.hpp"
#include "parse/front_matter.hpp"

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace mizuki {

Document read_document(std::filesystem::path const& path, DocumentKind kind);

struct Heading {
    unsigned level;
    std::string id;
    std::string text;
};

struct MarkdownFeatures {
    bool code{false};
    bool math{false};
    bool footnotes{false};
    bool admonitions{false};
};

struct RenderedMarkdown {
    std::string html;
    std::vector<Heading> headings;
    MarkdownFeatures features;
    std::vector<std::string> code_languages;
};

RenderedMarkdown render_markdown(std::string_view body, MarkdownOptions const& options = {});

} // namespace mizuki
