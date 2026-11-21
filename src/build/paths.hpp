#pragma once

#include <filesystem>

namespace mizuki {

struct SiteConfig;

// Validate before creating any build directories; return the resolved output path.
std::filesystem::path validate_output_path(SiteConfig const& options);

} // namespace mizuki
