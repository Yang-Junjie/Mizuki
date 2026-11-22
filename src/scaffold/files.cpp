#include "scaffold/files.hpp"

#include "util/path.hpp"

#include <spdlog/spdlog.h>

#include <cerrno>
#include <fstream>
#include <stdexcept>
#include <system_error>

namespace mizuki {
namespace fs = std::filesystem;

FileCreation::~FileCreation() {
    if (committed_) {
        return;
    }
    for (auto it = created_.rbegin(); it != created_.rend(); ++it) {
        std::error_code error;
        fs::remove(*it, error);
        if (error) {
            spdlog::warn("Failed to remove newly created path {}: {}", path_to_utf8(*it),
                         error.message());
        }
    }
}

void FileCreation::create_directory(fs::path const& path) {
    if (path.empty() || fs::is_directory(path)) {
        return;
    }
    create_directory(path.parent_path());
    if (fs::create_directory(path)) {
        created_.push_back(path);
    }
}

void FileCreation::write(fs::path const& path, std::string_view content) {
    create_directory(path.parent_path());
    // The exclusive open also protects against a file appearing after validation.
    std::ofstream file(path, std::ios::binary | std::ios::noreplace);
    if (!file.is_open()) {
        auto const error = std::error_code(errno, std::generic_category());
        throw fs::filesystem_error("Failed to create new file without overwriting", path, error);
    }
    created_.push_back(path);
    file.write(content.data(), static_cast<std::streamsize>(content.size()));
    file.close();
    if (!file) {
        throw std::runtime_error("Failed to write new file: " + path_to_utf8(path));
    }
}

void FileCreation::commit() {
    committed_ = true;
}

} // namespace mizuki
