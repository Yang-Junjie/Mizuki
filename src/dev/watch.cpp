#include "dev/watch.hpp"
#include "dev/file_watcher.hpp"

#include <spdlog/spdlog.h>

#include <atomic>
#include <chrono>
#include <thread>

namespace mizuki {

int watch(BuildOptions const& options) {
    std::atomic<bool> changed{true};
    auto on_change = [&changed](std::filesystem::path const&) { changed.store(true); };
    FileWatcher content_watcher(options.content_dir, on_change);
    FileWatcher theme_watcher(options.theme_dir, on_change);
    if (!content_watcher.valid() || !theme_watcher.valid()) {
        spdlog::error("Failed to watch {} or {}", options.content_dir.string(),
                      options.theme_dir.string());
        return 1;
    }

    Builder builder(options);
    spdlog::info("Watching {} and {}. Press Ctrl+C to stop.", options.content_dir.string(),
                 options.theme_dir.string());
    while (true) {
        // Only the main thread builds; callbacks just mark that something changed.
        if (changed.exchange(false)) {
            auto const result = builder.build();
            if (result.success) {
                spdlog::info("Built {} page(s), copied {} asset(s) -> {}", result.pages_built,
                             result.assets_copied, options.output_dir.string());
            } else {
                spdlog::error("Build failed: {}", result.error);
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
}

}  // namespace mizuki
