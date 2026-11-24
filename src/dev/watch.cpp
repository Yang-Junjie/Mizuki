#include "dev/watch.hpp"

#include "build/builder.hpp"
#include "build/paths.hpp"
#include "util/path.hpp"

#include <spdlog/spdlog.h>

#include <chrono>
#include <exception>
#include <thread>
#include <utility>

namespace mizuki {
namespace fs = std::filesystem;
namespace {

constexpr auto kPollInterval = std::chrono::milliseconds(200);

bool same_path(fs::path const& left, fs::path const& right) {
    return fs::absolute(left).lexically_normal() == fs::absolute(right).lexically_normal();
}

} // namespace

WatchSession::WatchSession(ConfigRequest request) : request_(std::move(request)) {
    request_.config_file = fs::absolute(request_.config_file).lexically_normal();
    // Watch the parent so atomic saves, deletion and recreation are all observed.
    config_watcher_ = std::make_unique<FileWatcher>(
        request_.config_file.parent_path(),
        [this](fs::path const& path) {
            if (same_path(path, request_.config_file)) {
                changed_.store(true);
            }
        },
        WatchDepth::Directory);
    update_watchers(load_config(request_));
}

void WatchSession::update_watchers(SiteConfig const& config) {
    validate_output_path(config);
    bool const content_changed =
        !content_watcher_ || !same_path(config.content_dir, watched_config_.content_dir);
    bool const theme_changed =
        !theme_watcher_ || !same_path(config.theme_dir, watched_config_.theme_dir);
    auto make_watcher = [this](fs::path const& directory) {
        return std::make_unique<FileWatcher>(directory,
                                             [this](fs::path const&) { changed_.store(true); });
    };
    // Prepare both before replacing either existing watcher.
    auto content = content_changed ? make_watcher(config.content_dir) : nullptr;
    auto theme = theme_changed ? make_watcher(config.theme_dir) : nullptr;
    if (content) {
        content_watcher_ = std::move(content);
    }
    if (theme) {
        theme_watcher_ = std::move(theme);
    }
    watched_config_ = config;
}

SiteConfig const& WatchSession::preview_config() const {
    return published_config_ ? *published_config_ : watched_config_;
}

void WatchSession::build_if_changed(OnBuild const& on_build) {
    if (!changed_.exchange(false)) {
        return;
    }
    try {
        auto const candidate = load_config(request_);
        // Keep watching failed candidates so fixing a new input triggers recovery.
        update_watchers(candidate);
        auto const stats = build_site(candidate);
        bool const preview_changed =
            !published_config_ || !same_path(candidate.output_dir, published_config_->output_dir) ||
            candidate.base_url.path != published_config_->base_url.path;
        published_config_ = candidate;
        spdlog::info("Built {} page(s), copied {} asset(s) -> {}", stats.pages_built,
                     stats.assets_copied, path_to_utf8(candidate.output_dir));
        if (on_build && (stats.published || preview_changed)) {
            on_build(*published_config_);
        }
    } catch (std::exception const& error) {
        spdlog::error("Build failed: {}", error.what());
    }
}

void WatchSession::run(std::stop_token stop, OnBuild const& on_build) {
    while (!stop.stop_requested()) {
        build_if_changed(on_build);
        std::this_thread::sleep_for(kPollInterval);
    }
}

int watch(ConfigRequest const& request) {
    WatchSession session(request);
    spdlog::info("Watching content, theme and configuration. Press Ctrl+C to stop.");
    session.run();
    return 0;
}

} // namespace mizuki
