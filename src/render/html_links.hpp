#pragma once

#include <functional>
#include <string>
#include <string_view>

namespace mizuki {

using LinkRewriter = std::function<std::string(std::string_view)>;

// Rewrites anchor hrefs, preserving text, comments and raw element contents.
std::string rewrite_anchor_links(std::string_view html, LinkRewriter const& rewrite);

} // namespace mizuki
