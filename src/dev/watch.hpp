#pragma once

#include "config/config.hpp"
#include "dev/file_watcher.hpp"

#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <stop_token>

namespace mizuki {

class WatchSession {
public:
    using OnBuild = std::function<void(SiteConfig const&)>;
    using Build = std::function<void(SiteConfig const&, std::set<std::filesystem::path> const&)>;

    explicit WatchSession(ConfigRequest request, Build build = {});

    // An existing output may be previewed if the first build fails.
    SiteConfig const& preview_config() const;
    void build_if_changed(OnBuild const& on_build = {});
    void run(std::stop_token stop = {}, OnBuild const& on_build = {});

private:
    void update_watchers(SiteConfig const& config);
    void mark_changed(std::filesystem::path const& path);

    ConfigRequest request_;
    Build build_;
    SiteConfig watched_config_;
    std::optional<SiteConfig> published_config_;
    std::mutex changes_mutex_;
    std::set<std::filesystem::path> changed_paths_;
    bool changed_{true};
    std::unique_ptr<FileWatcher> content_watcher_;
    std::unique_ptr<FileWatcher> theme_watcher_;
    std::unique_ptr<FileWatcher> config_watcher_;
};

int watch(ConfigRequest const& request);

} // namespace mizuki
