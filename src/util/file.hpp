#pragma once
#include <filesystem>
#include <string>
#include <string_view>

namespace mizuki {
std::string read_file(std::filesystem::path const& path);
void write_file(std::filesystem::path const& path, std::string_view content);
}  // namespace mizuki
