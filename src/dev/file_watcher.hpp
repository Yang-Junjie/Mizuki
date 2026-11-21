#pragma once

#include <filesystem>
#include <functional>
#include <memory>

namespace mizuki {

enum class WatchDepth { Directory, Recursive };

class FileWatcher {
public:
    using Callback = std::function<void(std::filesystem::path const&)>;

    FileWatcher(std::filesystem::path const& directory, Callback callback,
                WatchDepth depth = WatchDepth::Recursive);
    ~FileWatcher();

    FileWatcher(FileWatcher const&) = delete;
    FileWatcher& operator=(FileWatcher const&) = delete;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace mizuki
