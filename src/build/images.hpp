#pragma once

#include "build/artifact_cache.hpp"
#include "config/images.hpp"

#include <cstddef>
#include <filesystem>

namespace mizuki {

class ImageWriter {
public:
    ImageWriter(ImageOptions options, std::filesystem::path cache, std::filesystem::path stage);

    void write(std::filesystem::path const& source, std::filesystem::path const& destination);
    void log_summary() const;

private:
    ImageOptions options_;
    ArtifactCache cache_;
    std::size_t optimized_{0};
    std::size_t reused_{0};
    std::size_t bytes_saved_{0};
};

} // namespace mizuki
