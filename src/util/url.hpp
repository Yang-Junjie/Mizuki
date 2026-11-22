#pragma once

#include <filesystem>
#include <string>
#include <string_view>

namespace mizuki {

// path is URL-encoded, starts and ends with '/', and never contains dot segments.
struct BaseUrl {
    std::string origin;
    std::string path{"/"};
};

BaseUrl parse_base_url(std::string_view url);
// output_path is unencoded and relative to the output root; empty denotes the homepage.
// Returns empty when base has no origin.
std::string published_url(BaseUrl const& base, std::string_view output_path);

// Both paths are relative to the output root; source names an output file.
std::string relative_url(std::filesystem::path const& target, std::filesystem::path const& source);

std::string encode_url_path(std::string_view path);
std::string decode_url_path(std::string_view path);
bool has_url_scheme(std::string_view url);

} // namespace mizuki
