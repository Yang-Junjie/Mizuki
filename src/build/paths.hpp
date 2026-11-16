#pragma once

#include <filesystem>

namespace mizuki {

struct BuildOptions;

// Validate before creating any build directories; return the resolved output path.
std::filesystem::path validate_output_path(BuildOptions const& options);

}  // namespace mizuki
