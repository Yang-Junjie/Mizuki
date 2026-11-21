#include "build/plan.hpp"
#include "util/path.hpp"
#include "util/url.hpp"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <stdexcept>

namespace mizuki {
namespace fs = std::filesystem;
namespace {

std::string portable_key(fs::path const& path) {
    auto key = url_path(path.lexically_normal());
    // Reject ASCII case aliases on every platform so a site is portable to Windows.
    for (auto& ch : key) {
        if (ch >= 'A' && ch <= 'Z') ch = static_cast<char>(ch - 'A' + 'a');
    }
    return key;
}

class OutputRegistry {
public:
    explicit OutputRegistry(BuildOptions const& options) : output_(options.output_dir) {}

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
        throw std::runtime_error("Output path conflict\n  Output: " + url_path(output_ / path) +
                                 "\n  Source: " + previous + "\n  Source: " + source);
    }
    fs::path output_;
    std::map<std::string, std::string> files_;
    std::map<std::string, std::string> directories_;
};

std::vector<fs::path> regular_files(fs::path const& directory) {
    std::vector<fs::path> files;
    for (auto const& entry : fs::recursive_directory_iterator(directory)) {
        if (entry.is_regular_file()) files.push_back(entry.path());
    }
    std::ranges::sort(files);
    return files;
}

}  // namespace

BuildPlan plan_build(BuildOptions const& options) {
    BuildPlan plan;
    OutputRegistry registry(options);
    auto reserve = [&](fs::path const& path, std::string const& source) {
        registry.add(path, source);
        plan.outputs.emplace(path, source);
    };
    reserve("index.html", "generated homepage (" +
                              url_path(options.theme_dir / "layouts/home.html") + ")");
    plan.page_urls.emplace("index.md", "index.html");

    for (auto const& source : regular_files(options.content_dir)) {
        auto const relative = source.lexically_relative(options.content_dir);
        if (relative == fs::path("index.md")) continue;
        auto destination = relative;
        if (is_markdown(source)) {
            bool const independent = *relative.begin() == fs::path("pages");
            Markdown markdown(source, !independent);
            if (!markdown.load()) {
                throw std::runtime_error("Failed to parse " + url_path(source) + ": " + markdown.error());
            }
            if (markdown.front_matter().draft) {
                spdlog::info("Skipping draft: {}", url_path(source));
                continue;
            }
            destination.replace_extension(".html");
            reserve(destination, url_path(source));
            plan.page_urls.emplace(relative, destination);
            plan.pages.push_back({source, relative, destination, std::move(markdown), independent});
        } else {
            reserve(destination, url_path(source));
            plan.assets.push_back({source, destination});
        }
    }
    auto const theme_assets = options.theme_dir / "assets";
    if (fs::exists(theme_assets)) {
        for (auto const& source : regular_files(theme_assets)) {
            auto const destination = fs::path("assets") / source.lexically_relative(theme_assets);
            reserve(destination, url_path(source));
            plan.assets.push_back({source, destination});
        }
    }
    return plan;
}

}  // namespace mizuki
