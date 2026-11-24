#pragma once

#include <filesystem>
#include <string_view>

namespace mizuki {

struct SiteConfig;

// Validate before creating any build directories; return the resolved output path.
std::filesystem::path validate_output_path(SiteConfig const& options);
std::filesystem::path cache_path(SiteConfig const& options, std::filesystem::path const& output,
                                 std::string_view name);

} // namespace mizuki
