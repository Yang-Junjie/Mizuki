#include "build/output_transaction.hpp"

#include "util/path.hpp"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstring>
#include <fstream>
#include <map>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

#ifdef _WIN32
#include <windows.h>
#endif

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

void report_rename_lock(fs::path const& output, std::error_code const& error) {
#ifdef _WIN32
    if (error == std::errc::permission_denied ||
        (error.category() == std::system_category() &&
         (error.value() == ERROR_SHARING_VIOLATION || error.value() == ERROR_LOCK_VIOLATION))) {
        spdlog::warn("Windows may be holding the output directory open: {}. Close it in File "
                     "Explorer or any other program and retry the build.",
                     path_to_utf8(output));
    }
#else
    (void)output;
    (void)error;
#endif
}

bool files_match(fs::path const& left, fs::path const& right) {
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

bool trees_match(fs::path const& stage, fs::path const& published,
                 std::span<PlannedAsset const> pending_copies) {
    if (fs::symlink_status(published).type() != fs::file_type::directory) {
        return false;
    }
    // Check generated pages before comparing pending copies. Those assets already have
    // their final bytes at the source, so there is no need to stage them for comparison.
    struct ExpectedEntry {
        fs::file_type type{fs::file_type::directory};
        bool staged{false};
    };
    std::map<fs::path, ExpectedEntry> expected;
    for (auto const& entry : fs::recursive_directory_iterator(stage)) {
        auto const type = entry.symlink_status().type();
        if (type != fs::file_type::directory && type != fs::file_type::regular) {
            return false;
        }
        expected.emplace(entry.path().lexically_relative(stage), ExpectedEntry{type, true});
    }
    for (auto const& asset : pending_copies) {
        expected.emplace(asset.output, ExpectedEntry{fs::file_type::regular, false});
        for (auto parent = asset.output.parent_path(); !parent.empty();
             parent = parent.parent_path()) {
            if (!expected.try_emplace(parent).second) {
                break;
            }
        }
    }
    std::size_t seen = 0;
    for (auto const& entry : fs::recursive_directory_iterator(published)) {
        auto const found = expected.find(entry.path().lexically_relative(published));
        if (found == expected.end()) {
            return false;
        }
        auto const& [expected_type, staged] = found->second;
        auto const type = entry.symlink_status().type();
        if (type != expected_type || (staged && type == fs::file_type::regular &&
                                      !files_match(stage / found->first, entry.path()))) {
            return false;
        }
        ++seen;
    }
    return seen == expected.size() && std::ranges::all_of(pending_copies, [&](auto const& asset) {
               return files_match(asset.source, published / asset.output);
           });
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

bool OutputTransaction::matches_published(std::span<PlannedAsset const> pending_copies) const {
    try {
        return trees_match(stage_, output_, pending_copies);
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
            report_rename_lock(output_, error);
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
        report_rename_lock(output_, publish_error);
        throw std::runtime_error(message);
    }

    stage_.clear();
    cleanup_directory(backup);
}

} // namespace mizuki
