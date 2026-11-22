#pragma once

#include <functional>
#include <string>
#include <string_view>

namespace mizuki {

enum class HtmlLinkKind { Anchor, Resource };
using LinkRewriter = std::function<std::string(std::string_view, HtmlLinkKind)>;

// Rewrites document URL attributes, preserving text, comments and raw element contents.
std::string rewrite_document_links(std::string_view html, LinkRewriter const& rewrite);

} // namespace mizuki
