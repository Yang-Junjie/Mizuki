#include "dev/file_watcher.hpp"

#include "util/path.hpp"

#include <efsw/efsw.hpp>

#include <stdexcept>
#include <string>
#include <utility>

namespace mizuki {
namespace fs = std::filesystem;

struct FileWatcher::Impl final : efsw::FileWatchListener {
    Impl(fs::path const& directory, Callback callback, WatchDepth depth)
        : callback(std::move(callback)) {
        id = watcher.addWatch(path_to_utf8(directory), this, depth == WatchDepth::Recursive);
        if (id < 0) {
            throw std::runtime_error("Failed to watch " + path_to_utf8(directory));
        }
        watcher.watch();
    }

    ~Impl() override { watcher.removeWatch(id); }

    void handleFileAction(efsw::WatchID, std::string const& directory, std::string const& filename,
                          efsw::Action, std::string const&) override {
        if (callback) {
            callback(path_from_utf8(directory) / path_from_utf8(filename));
        }
    }

    Callback callback;
    efsw::FileWatcher watcher;
    efsw::WatchID id;
};

FileWatcher::FileWatcher(fs::path const& directory, Callback callback, WatchDepth depth)
    : impl_(std::make_unique<Impl>(directory, std::move(callback), depth)) {}

FileWatcher::~FileWatcher() = default;

} // namespace mizuki
