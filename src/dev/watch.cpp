#include "dev/watch.hpp"
#include "build/paths.hpp"
#include "util/url.hpp"

#include <spdlog/spdlog.h>

#include <chrono>
#include <stdexcept>
#include <thread>
#include <utility>

namespace mizuki {
namespace fs = std::filesystem;

WatchSession::WatchSession(ConfigRequest request, std::function<void(BuildOptions const&)> on_build)
    : request_(std::move(request)), on_build_(std::move(on_build)) {
    request_.config_file = fs::absolute(request_.config_file).lexically_normal();
    auto const initial = load_config(request_);
    update_watchers(initial);
    // Watch the parent so atomic saves, deletion and recreation are all observed.
    config_watcher_ = std::make_unique<FileWatcher>(
        request_.config_file.parent_path(), [this](fs::path const& path) {
            if (fs::absolute(path).lexically_normal() == request_.config_file) changed_.store(true);
        }, false);
    if (!valid()) throw std::runtime_error("Failed to watch configuration directory");
}

void WatchSession::update_watchers(BuildOptions const& options) {
    validate_output_path(options);
    bool const content_changed = !content_watcher_ ||
        fs::absolute(options.content_dir).lexically_normal() != fs::absolute(options_.content_dir).lexically_normal();
    bool const theme_changed = !theme_watcher_ ||
        fs::absolute(options.theme_dir).lexically_normal() != fs::absolute(options_.theme_dir).lexically_normal();
    auto make_watcher = [this](fs::path const& directory) {
        auto watcher = std::make_unique<FileWatcher>(directory, [this](fs::path const&) { changed_.store(true); });
        if (!watcher->valid()) throw std::runtime_error("Failed to watch " + url_path(directory));
        return watcher;
    };
    // Prepare both before replacing either existing watcher.
    auto content = content_changed ? make_watcher(options.content_dir) : nullptr;
    auto theme = theme_changed ? make_watcher(options.theme_dir) : nullptr;
    if (content) content_watcher_ = std::move(content);
    if (theme) theme_watcher_ = std::move(theme);
    options_ = options;
}

bool WatchSession::valid() const {
    return content_watcher_ && content_watcher_->valid() && theme_watcher_ && theme_watcher_->valid() &&
           config_watcher_ && config_watcher_->valid();
}

BuildOptions const& WatchSession::options() const { return options_; }

void WatchSession::build_if_changed() {
    if (!changed_.exchange(false)) return;
    try {
        auto const candidate = load_config(request_);
        update_watchers(candidate);
        auto const result = Builder(candidate).build();
        if (!result.success) {
            spdlog::error("Build failed: {}", result.error);
            return;
        }
        spdlog::info("Built {} page(s), copied {} asset(s) -> {}", result.pages_built,
                     result.assets_copied, url_path(candidate.output_dir));
        if (on_build_) on_build_(candidate);
    } catch (std::exception const& error) {
        spdlog::error("Build failed: {}", error.what());
    }
}

void WatchSession::run(std::stop_token stop) {
    while (!stop.stop_requested()) {
        build_if_changed();
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
}

int watch(ConfigRequest const& request) {
    WatchSession session(request);
    spdlog::info("Watching content, theme and configuration. Press Ctrl+C to stop.");
    session.run();
    return 0;
}

}  // namespace mizuki
