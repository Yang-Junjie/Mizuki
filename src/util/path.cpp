#include "util/path.hpp"

#include <algorithm>
#include <cctype>

namespace mizuki {

bool is_markdown(std::filesystem::path const& path) {
    auto extension = path.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return extension == ".md" || extension == ".markdown";
}

}  // namespace mizuki
