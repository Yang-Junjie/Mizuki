#pragma once

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace mizuki {

std::string sha256(std::string_view bytes);

// Entries are content-addressed and verified before use. Cache I/O is best effort.
class ArtifactCache {
public:
    ArtifactCache(std::filesystem::path root, std::filesystem::path const& stage);

    bool enabled() const;
    std::optional<std::string> read(std::string_view key,
                                    std::size_t max_size = 64 * 1024 * 1024) const;
    void write(std::string_view key, std::string_view content) const;

private:
    std::filesystem::path root_;
    std::filesystem::path temporary_suffix_;
};

} // namespace mizuki
