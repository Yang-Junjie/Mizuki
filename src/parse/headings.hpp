#pragma once

#include "parse/markdown.hpp"

namespace mizuki {

// Reads actual HTML headings, preserving authored IDs and ignoring code and raw text elements.
RenderedMarkdown add_heading_anchors(std::string_view html);

} // namespace mizuki
