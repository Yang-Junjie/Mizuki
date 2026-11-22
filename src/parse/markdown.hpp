#pragma once

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

struct RenderedMarkdown {
    std::string html;
    std::vector<Heading> headings;
};

RenderedMarkdown render_markdown(std::string_view body);

} // namespace mizuki
