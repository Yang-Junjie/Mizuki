#include "build/paths.hpp"

#include "config/site.hpp"
#include "util/path.hpp"

#include <stdexcept>
#include <system_error>
#include <utility>

namespace mizuki {

namespace fs = std::filesystem;

namespace {

// Walk actual ancestors as well as comparing paths, so aliases on case-insensitive
// filesystems cannot bypass the directory overlap checks.
bool is_within(fs::path path, fs::path const& directory) {
    for (;;) {
        std::error_code error;
        if (path == directory || fs::equivalent(path, directory, error)) {
            return true;
        }
        auto parent = path.parent_path();
        if (parent == path || parent.empty()) {
            return false;
        }
        path = std::move(parent);
    }
}

} // namespace

fs::path validate_output_path(SiteConfig const& options) {
    if (options.output_dir.empty()) {
        throw std::runtime_error("Output directory must not be empty");
    }

    auto requested = fs::absolute(options.output_dir);
    // Preserve a final symlink check even when the argument ends in '/' or '/.'.
    while (requested.has_relative_path() &&
           (requested.filename().empty() || requested.filename() == ".")) {
        requested = requested.parent_path();
    }
    auto const status = fs::symlink_status(requested);
    if (fs::is_symlink(status)) {
        throw std::runtime_error("Output directory must not be a symbolic link: " +
                                 path_to_utf8(requested));
    }
    if (fs::exists(status) && !fs::is_directory(status)) {
        throw std::runtime_error("Output path must be a directory: " + path_to_utf8(requested));
    }

    auto output = fs::weakly_canonical(requested);
    // A trailing separator is irrelevant to containment and sibling names.
    while (output.has_relative_path() && output.filename().empty()) {
        output = output.parent_path();
    }
    if (output == output.root_path() || is_within(fs::canonical(fs::current_path()), output)) {
        throw std::runtime_error("Output directory must not be a filesystem root, the working "
                                 "directory or one of its ancestors: " +
                                 path_to_utf8(output));
    }
    if (!options.config_file.empty() &&
        is_within(fs::weakly_canonical(options.config_file), output)) {
        throw std::runtime_error("Output directory must not contain the configuration file: " +
                                 path_to_utf8(options.config_file));
    }
    for (auto const& source : {options.content_dir, options.theme_dir}) {
        if (!fs::is_directory(source)) {
            throw std::runtime_error("Source directory does not exist or is not a directory: " +
                                     path_to_utf8(source));
        }
        auto const canonical_source = fs::canonical(source);
        if (is_within(output, canonical_source) || is_within(canonical_source, output)) {
            throw std::runtime_error("Output directory must not overlap source directory: " +
                                     path_to_utf8(canonical_source));
        }
    }
    return output;
}

fs::path cache_path(SiteConfig const& options, fs::path const& output, std::string_view name) {
    std::error_code error;
    auto const cache = fs::weakly_canonical(output.parent_path() / ".cache/mizuki" / name, error);
    if (error || is_within(cache, output) || is_within(output, cache)) {
        return {};
    }
    for (auto const& source : {options.content_dir, options.theme_dir}) {
        auto const canonical_source = fs::canonical(source);
        if (is_within(cache, canonical_source) || is_within(canonical_source, cache)) {
            return {};
        }
    }
    return cache;
}

} // namespace mizuki
