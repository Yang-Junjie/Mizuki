#include "scaffold/content.hpp"

#include "config/site.hpp"
#include "parse/front_matter.hpp"
#include "scaffold/files.hpp"
#include "util/date.hpp"
#include "util/path.hpp"

#include <algorithm>
#include <stdexcept>
#include <string_view>

namespace mizuki {
namespace fs = std::filesystem;
namespace {

void validate_text(std::string_view value, std::string_view name, bool required) {
    if (required && value.find_first_not_of(" \t") == std::string_view::npos) {
        throw std::runtime_error(std::string(name) + " must not be empty");
    }
    if (std::ranges::any_of(
            value, [](unsigned char ch) { return (ch < 0x20 && ch != '\t') || ch == 0x7f; })) {
        throw std::runtime_error(std::string(name) +
                                 " must be single-line text without control characters");
    }
}

fs::path content_path(SiteConfig const& config, fs::path const& path) {
    if (path.empty() || path.has_root_path() ||
        std::ranges::find(path, fs::path("..")) != path.end()) {
        throw std::runtime_error(
            "Content path must be relative to the content directory without '..'");
    }
    auto const relative = path.lexically_normal();
    if (!is_markdown(relative)) {
        throw std::runtime_error("Content path must end in .md: " + path_to_utf8(path));
    }
    auto name = path_to_utf8(relative);
    std::ranges::transform(name, name.begin(), [](char ch) {
        return ch >= 'A' && ch <= 'Z' ? static_cast<char>(ch - 'A' + 'a') : ch;
    });
    if (name == "index.md") {
        throw std::runtime_error("index.md is reserved for the generated homepage");
    }
    auto const root = fs::weakly_canonical(fs::absolute(config.content_dir));
    auto current = root;
    for (auto const& component : relative) {
        current /= component;
        if (fs::is_symlink(fs::symlink_status(current))) {
            throw std::runtime_error("Content path must not pass through a symbolic link: " +
                                     path_to_utf8(current));
        }
    }
    if (fs::exists(fs::symlink_status(current))) {
        throw std::runtime_error("Content path already exists: " + path_to_utf8(current));
    }
    return current;
}

} // namespace

fs::path create_content(SiteConfig const& config, NewContent const& content) {
    validate_text(content.title, "Title", true);
    validate_text(content.description, "Description", false);
    auto const path = content_path(config, content.path);
    // Front matter is literal single-line text: quotes inside a value are not escapes.
    auto source = "---\ntitle: \"" + content.title + "\"\ndate: " + local_date() +
                  "\ndescription: \"" + content.description +
                  "\"\ndraft: " + (content.draft ? "true" : "false") + "\n---\n\n";
    parse_document(source, DocumentKind::Article);
    FileCreation files;
    files.write(path, source);
    files.commit();
    return path;
}

} // namespace mizuki
