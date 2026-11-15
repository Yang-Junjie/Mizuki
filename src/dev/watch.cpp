#include "dev/watch.hpp"

#include <spdlog/spdlog.h>

#include <chrono>
#include <thread>
#include <utility>

namespace mizuki {

WatchSession::WatchSession(BuildOptions options)
    : options_(std::move(options)), builder_(options_),
      content_watcher_(options_.content_dir,
                       [this](std::filesystem::path const&) { changed_.store(true); }),
      theme_watcher_(options_.theme_dir,
                     [this](std::filesystem::path const&) { changed_.store(true); }) {
    if (!valid()) {
        spdlog::error("Failed to watch {} or {}", options_.content_dir.string(),
                      options_.theme_dir.string());
    }
}

bool WatchSession::valid() const {
    return content_watcher_.valid() && theme_watcher_.valid();
}

void WatchSession::build_if_changed() {
    // Clear before building so edits made during the build remain pending.
    if (!changed_.exchange(false)) {
        return;
    }
    auto const result = builder_.build();
    if (result.success) {
        spdlog::info("Built {} page(s), copied {} asset(s) -> {}", result.pages_built,
                     result.assets_copied, options_.output_dir.string());
    } else {
        spdlog::error("Build failed: {}", result.error);
    }
}

void WatchSession::run(std::stop_token stop) {
    while (!stop.stop_requested()) {
        build_if_changed();
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
}

int watch(BuildOptions const& options) {
    WatchSession session(options);
    if (!session.valid()) {
        return 1;
    }
    spdlog::info("Watching {} and {}. Press Ctrl+C to stop.", options.content_dir.string(),
                 options.theme_dir.string());
    session.run();
    return 0;
}

}  // namespace mizuki
