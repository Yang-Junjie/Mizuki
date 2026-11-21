#pragma once

#include "parse/front_matter.hpp"

#include <filesystem>
#include <string>
#include <string_view>

namespace mizuki {

Document read_document(std::filesystem::path const& path, DocumentKind kind);
std::string render_markdown(std::string_view body);

} // namespace mizuki
