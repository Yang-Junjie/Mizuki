#pragma once

#include "config/config.hpp"
#include "dev/file_watcher.hpp"

#include <atomic>
#include <functional>
#include <memory>
#include <optional>
#include <stop_token>

namespace mizuki {

class WatchSession {
public:
    using OnBuild = std::function<void(SiteConfig const&)>;

    explicit WatchSession(ConfigRequest request);

    // An existing output may be previewed if the first build fails.
    SiteConfig const& preview_config() const;
    void build_if_changed(OnBuild const& on_build = {});
    void run(std::stop_token stop = {}, OnBuild const& on_build = {});

private:
    void update_watchers(SiteConfig const& config);

    ConfigRequest request_;
    SiteConfig watched_config_;
    std::optional<SiteConfig> published_config_;
    std::atomic<bool> changed_{true};
    std::unique_ptr<FileWatcher> content_watcher_;
    std::unique_ptr<FileWatcher> theme_watcher_;
    std::unique_ptr<FileWatcher> config_watcher_;
};

int watch(ConfigRequest const& request);

} // namespace mizuki
