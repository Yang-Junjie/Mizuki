#pragma once

#include <filesystem>
#include <string>
#include <string_view>

namespace mizuki {

std::string url_path(std::filesystem::path const& path);
std::filesystem::path path_from_url(std::string_view path);
std::string encode_url_path(std::string_view path);
std::string decode_url_path(std::string_view path);
bool has_url_scheme(std::string_view url);

}  // namespace mizuki
