#pragma once

#include <string>
#include <string_view>

namespace mizuki {

std::string encode_url_path(std::string_view path);
std::string decode_url_path(std::string_view path);
bool has_url_scheme(std::string_view url);

} // namespace mizuki
