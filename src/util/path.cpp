#include "util/path.hpp"

#include <algorithm>
#include <cctype>

namespace mizuki {

std::string path_to_utf8(std::filesystem::path const& path) {
    auto const utf8 = path.generic_u8string();
    return {reinterpret_cast<char const*>(utf8.data()), utf8.size()};
}

std::filesystem::path path_from_utf8(std::string_view path) {
    return std::filesystem::path(
        std::u8string_view(reinterpret_cast<char8_t const*>(path.data()), path.size()));
}

bool is_markdown(std::filesystem::path const& path) {
    auto extension = path_to_utf8(path.extension());
    std::transform(extension.begin(), extension.end(), extension.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return extension == ".md";
}

} // namespace mizuki
