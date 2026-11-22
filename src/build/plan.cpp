#include "build/plan.hpp"

#include "parse/markdown.hpp"
#include "util/path.hpp"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace mizuki {
namespace fs = std::filesystem;
namespace {

std::string portable_key(fs::path const& path) {
    auto key = path_to_utf8(path.lexically_normal());
    // Reject ASCII case aliases on every platform so a site is portable to Windows.
    for (auto& ch : key) {
        if (ch >= 'A' && ch <= 'Z') {
            ch = static_cast<char>(ch - 'A' + 'a');
        }
    }
    return key;
}

class OutputRegistry {
public:
    explicit OutputRegistry(SiteConfig const& options) : output_(options.output_dir) {}

    void add(fs::path const& path, std::string const& source) {
        auto const key = portable_key(path);
        if (auto previous = files_.find(key); previous != files_.end()) {
            conflict(path, source, previous->second);
        }
        if (auto previous = directories_.find(key); previous != directories_.end()) {
            conflict(path, source, previous->second);
        }
        for (auto parent = path.parent_path(); !parent.empty(); parent = parent.parent_path()) {
            auto const parent_key = portable_key(parent);
            if (auto previous = files_.find(parent_key); previous != files_.end()) {
                conflict(parent, source, previous->second);
            }
        }
        files_.emplace(key, source);
        for (auto parent = path.parent_path(); !parent.empty(); parent = parent.parent_path()) {
            directories_.try_emplace(portable_key(parent), source);
        }
    }

private:
    [[noreturn]] void conflict(fs::path const& path, std::string const& source,
                               std::string const& previous) const {
        throw std::runtime_error("Output path conflict\n  Output: " + path_to_utf8(output_ / path) +
                                 "\n  Source: " + previous + "\n  Source: " + source);
    }
    fs::path output_;
    std::map<std::string, std::string> files_;
    std::map<std::string, std::string> directories_;
};

std::vector<fs::path> regular_files(fs::path const& directory) {
    std::vector<fs::path> files;
    for (auto const& entry : fs::recursive_directory_iterator(directory)) {
        if (entry.is_regular_file()) {
            files.push_back(entry.path());
        }
    }
    std::ranges::sort(files);
    return files;
}

} // namespace

BuildPlan plan_build(SiteConfig const& options) {
    BuildPlan plan;
    plan.base_path = options.base_url.path;
    OutputRegistry registry(options);
    auto reserve = [&](fs::path const& path, std::string const& source) {
        registry.add(path, source);
        plan.outputs.insert(path);
    };
    reserve("index.html",
            "generated homepage (" + path_to_utf8(options.theme_dir / "layouts/home.html") + ")");
    plan.page_urls.emplace("index.md", "index.html");

    for (auto const& source : regular_files(options.content_dir)) {
        auto const relative = source.lexically_relative(options.content_dir);
        if (relative == fs::path("index.md")) {
            continue;
        }
        auto destination = relative;
        if (is_markdown(source)) {
            auto const kind =
                *relative.begin() == fs::path("pages") ? DocumentKind::Page : DocumentKind::Article;
            auto document = read_document(source, kind);
            if (document.metadata.draft) {
                spdlog::info("Skipping draft: {}", path_to_utf8(source));
                continue;
            }
            destination.replace_extension(".html");
            reserve(destination, path_to_utf8(source));
            plan.page_urls.emplace(relative, destination);
            plan.pages.push_back({source, relative, destination, std::move(document), kind});
        } else {
            reserve(destination, path_to_utf8(source));
            plan.assets.push_back({source, destination});
        }
    }
    auto const theme_assets = options.theme_dir / "assets";
    if (fs::exists(theme_assets)) {
        for (auto const& source : regular_files(theme_assets)) {
            auto const destination = fs::path("assets") / source.lexically_relative(theme_assets);
            reserve(destination, path_to_utf8(source));
            plan.assets.push_back({source, destination});
        }
    }
    return plan;
}

} // namespace mizuki
