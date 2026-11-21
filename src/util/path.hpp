#pragma once
#include <filesystem>
#include <string>
#include <string_view>

namespace mizuki {

std::string path_to_utf8(std::filesystem::path const& path);
std::filesystem::path path_from_utf8(std::string_view path);
bool is_markdown(std::filesystem::path const& path);

} // namespace mizuki
