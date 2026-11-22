#pragma once

#include <string>
#include <string_view>

namespace mizuki {

// path is URL-encoded, starts and ends with '/', and never contains dot segments.
struct BaseUrl {
    std::string origin;
    std::string path{"/"};
};

BaseUrl parse_base_url(std::string_view url);

std::string encode_url_path(std::string_view path);
std::string decode_url_path(std::string_view path);
bool has_url_scheme(std::string_view url);

} // namespace mizuki
