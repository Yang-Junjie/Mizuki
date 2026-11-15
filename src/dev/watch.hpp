#pragma once
#include "build/builder.hpp"
#include "dev/file_watcher.hpp"

#include <atomic>
#include <stop_token>

namespace mizuki {

class WatchSession {
public:
    explicit WatchSession(BuildOptions options);

    bool valid() const;
    void build_if_changed();
    void run(std::stop_token stop = {});

private:
    BuildOptions options_;
    Builder builder_;
    std::atomic<bool> changed_{true};
    FileWatcher content_watcher_;
    FileWatcher theme_watcher_;
};

int watch(BuildOptions const& options);

}  // namespace mizuki
