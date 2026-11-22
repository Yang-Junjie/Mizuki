#pragma once

#include "parse/markdown.hpp"

namespace mizuki {

// Adds heading anchors and collects content features from HTML, ignoring opaque raw text.
RenderedMarkdown analyze_markdown_html(std::string_view html);

} // namespace mizuki
