#pragma once
#include <efsw/efsw.hpp>

#include <filesystem>
#include <functional>
#include <string>
#include <utility>

namespace mizuki {
class FileWatcher final : public efsw::FileWatchListener {
public:
    using Callback = std::function<void(std::filesystem::path const&)>;

    FileWatcher(std::filesystem::path directory, Callback callback, bool recursive = true)
        : callback_(std::move(callback)) {
        watch_id_ = watcher_.addWatch(directory.string(), this, recursive);
        watcher_.watch();
    }

    ~FileWatcher() override {
        if (watch_id_ >= 0) {
            watcher_.removeWatch(watch_id_);
        }
    }

    FileWatcher(FileWatcher const&) = delete;
    FileWatcher& operator=(FileWatcher const&) = delete;

    bool valid() const { return watch_id_ >= 0; }

private:
    void handleFileAction(efsw::WatchID, std::string const& dir, std::string const& filename,
                          efsw::Action, std::string const&) override {
        if (callback_) {
            callback_(std::filesystem::path(dir) / filename);
        }
    }

    efsw::FileWatcher watcher_;
    efsw::WatchID watch_id_{-1};
    Callback callback_;
};
}
