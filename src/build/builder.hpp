#pragma once

#include <cstddef>
#include <filesystem>
#include <string>

namespace mizuki {

struct BuildOptions {
    std::filesystem::path content_dir{"content"};
    std::filesystem::path theme_dir{"themes/default"};
    std::filesystem::path output_dir{"dist"};
};

struct BuildResult {
    bool success{false};
    std::size_t pages_built{0};
    std::size_t assets_copied{0};
    std::string error;
};

class Builder {
public:
    explicit Builder(BuildOptions options = {});

    BuildResult build() const;

private:
    BuildOptions options_;
};

}  // namespace mizuki
