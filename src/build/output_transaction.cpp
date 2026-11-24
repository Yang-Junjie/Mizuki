#include "build/output_transaction.hpp"

#include "util/path.hpp"

#include <spdlog/spdlog.h>

#include <array>
#include <atomic>
#include <chrono>
#include <cstring>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

namespace mizuki {

namespace fs = std::filesystem;

namespace {

fs::path create_unique_sibling(fs::path const& output, std::string_view kind) {
    static std::atomic<unsigned long long> sequence{0};
    for (int attempt = 0; attempt < 100; ++attempt) {
        auto candidate = output;
        candidate += ".mizuki-" + std::string(kind) + "-" +
                     std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) +
                     "-" + std::to_string(sequence.fetch_add(1, std::memory_order_relaxed));
        std::error_code error;
        if (fs::create_directory(candidate, error)) {
            return candidate;
        }
        if (error && error != std::errc::file_exists) {
            throw fs::filesystem_error("Failed to create temporary build directory", candidate,
                                       error);
        }
    }
    throw std::runtime_error("Failed to reserve a unique build directory beside " +
                             path_to_utf8(output));
}

void cleanup_directory(fs::path const& path) {
    if (path.empty()) {
        return;
    }
    std::error_code error;
    fs::remove_all(path, error);
    if (error) {
        spdlog::warn("Failed to remove build directory {}; remove it manually: {}",
                     path_to_utf8(path), error.message());
    }
}

bool files_match(fs::path const& left, fs::path const& right) {
    if (fs::file_size(left) != fs::file_size(right)) {
        return false;
    }
    std::ifstream a(left, std::ios::binary);
    std::ifstream b(right, std::ios::binary);
    if (!a || !b) {
        return false;
    }
    std::array<char, 64 * 1024> a_bytes;
    std::array<char, 64 * 1024> b_bytes;
    do {
        a.read(a_bytes.data(), a_bytes.size());
        b.read(b_bytes.data(), b_bytes.size());
        if (a.gcount() != b.gcount() ||
            std::memcmp(a_bytes.data(), b_bytes.data(), static_cast<std::size_t>(a.gcount())) !=
                0 ||
            a.bad() || b.bad() || a.eof() != b.eof()) {
            return false;
        }
    } while (a && b);
    return a.eof() && b.eof();
}

bool trees_match(fs::path const& stage, fs::path const& published) {
    if (fs::symlink_status(published).type() != fs::file_type::directory) {
        return false;
    }
    std::size_t entries = 0;
    for (auto const& expected : fs::recursive_directory_iterator(stage)) {
        auto const target = published / expected.path().lexically_relative(stage);
        auto const type = fs::symlink_status(target).type();
        if (type != expected.symlink_status().type() ||
            (type == fs::file_type::regular && !files_match(expected.path(), target))) {
            return false;
        }
        ++entries;
    }
    // Any additional published entry, including an empty directory, must be removed.
    auto const existing = fs::recursive_directory_iterator(published);
    return static_cast<std::size_t>(std::distance(existing, fs::recursive_directory_iterator{})) ==
           entries;
}

} // namespace

OutputTransaction::OutputTransaction(fs::path output) : output_(std::move(output)) {
    fs::create_directories(output_.parent_path());
    stage_ = create_unique_sibling(output_, "stage");
}

OutputTransaction::~OutputTransaction() {
    cleanup_directory(stage_);
}

fs::path const& OutputTransaction::stage() const {
    return stage_;
}

bool OutputTransaction::matches_published() const {
    try {
        return trees_match(stage_, output_);
    } catch (fs::filesystem_error const&) {
        return false;
    }
}

void OutputTransaction::commit() {
    fs::path backup;
    fs::path saved_site;
    if (fs::exists(output_)) {
        // Reserve a container rather than an existing rename destination;
        // Windows does not support renaming a directory over an empty one.
        backup = create_unique_sibling(output_, "backup");
        saved_site = backup / "site";
        std::error_code error;
        fs::rename(output_, saved_site, error);
        if (error) {
            cleanup_directory(backup);
            throw fs::filesystem_error("Failed to back up output directory", output_, saved_site,
                                       error);
        }
    }

    std::error_code publish_error;
    fs::rename(stage_, output_, publish_error);
    if (publish_error) {
        std::error_code restore_error;
        if (!backup.empty()) {
            fs::rename(saved_site, output_, restore_error);
            if (!restore_error) {
                cleanup_directory(backup);
            }
        }
        auto message = "Failed to publish output directory " + path_to_utf8(output_) + ": " +
                       publish_error.message();
        if (restore_error) {
            message += ". Failed to restore previous output: " + restore_error.message() +
                       ". Previous site preserved at: " + path_to_utf8(saved_site);
        }
        throw std::runtime_error(message);
    }

    stage_.clear();
    cleanup_directory(backup);
}

} // namespace mizuki
