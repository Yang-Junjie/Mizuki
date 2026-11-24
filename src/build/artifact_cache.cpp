#include "build/artifact_cache.hpp"

#include "util/file.hpp"

#include <spdlog/spdlog.h>

#include <exception>
#include <picosha2.h>
#include <system_error>
#include <utility>

namespace mizuki {
namespace fs = std::filesystem;

std::string sha256(std::string_view bytes) {
    return picosha2::hash256_hex_string(bytes.begin(), bytes.end());
}

ArtifactCache::ArtifactCache(fs::path root, fs::path const& stage)
    : root_(std::move(root)), temporary_suffix_(stage.filename()) {}

bool ArtifactCache::enabled() const {
    return !root_.empty();
}

std::optional<std::string> ArtifactCache::read(std::string_view key, std::size_t max_size) const {
    if (!enabled()) {
        return std::nullopt;
    }
    constexpr std::size_t digest_size = 64;
    try {
        auto const path = root_ / key;
        std::error_code error;
        auto const size = fs::file_size(path, error);
        if (error || size < digest_size || size - digest_size > max_size) {
            return std::nullopt;
        }
        auto entry = read_file(path);
        if (entry.size() != size) {
            return std::nullopt;
        }
        auto const payload = std::string_view(entry).substr(digest_size);
        if (sha256(payload) == std::string_view(entry).substr(0, digest_size)) {
            entry.erase(0, digest_size);
            return entry;
        }
    } catch (std::exception const& error) {
        spdlog::debug("Build cache read failed: {}", error.what());
    }
    return std::nullopt;
}

void ArtifactCache::write(std::string_view key, std::string_view content) const {
    if (!enabled()) {
        return;
    }
    auto const path = root_ / key;
    auto temporary = path;
    temporary += temporary_suffix_;
    try {
        fs::create_directories(root_);
        auto bytes = sha256(content);
        bytes.append(content);
        write_file(temporary, bytes);
        std::error_code error;
        fs::rename(temporary, path, error);
        if (!error) {
            return;
        }
    } catch (std::exception const& error) {
        spdlog::debug("Build cache write failed: {}", error.what());
    }
    std::error_code ignored;
    fs::remove(temporary, ignored);
}

} // namespace mizuki
