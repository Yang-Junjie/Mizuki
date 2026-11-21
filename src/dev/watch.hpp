#pragma once

#include "config/config.hpp"
#include "dev/file_watcher.hpp"

#include <atomic>
#include <functional>
#include <memory>
#include <stop_token>

namespace mizuki {

class WatchSession {
public:
    explicit WatchSession(ConfigRequest request,
                          std::function<void(BuildOptions const&)> on_build = {});

    bool valid() const;
    BuildOptions const& options() const;
    void build_if_changed();
    void run(std::stop_token stop = {});

private:
    void update_watchers(BuildOptions const& options);

    ConfigRequest request_;
    BuildOptions options_;
    std::function<void(BuildOptions const&)> on_build_;
    std::atomic<bool> changed_{true};
    std::unique_ptr<FileWatcher> content_watcher_;
    std::unique_ptr<FileWatcher> theme_watcher_;
    std::unique_ptr<FileWatcher> config_watcher_;
};

int watch(ConfigRequest const& request);

}  // namespace mizuki
