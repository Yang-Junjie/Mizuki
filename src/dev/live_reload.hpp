#pragma once

#include <condition_variable>
#include <cstddef>
#include <filesystem>
#include <memory>
#include <mutex>
#include <string>

namespace httplib {
struct Response;
}

namespace mizuki {

struct PreviewFiles;

struct PreviewSnapshot {
    std::filesystem::path output_dir;
    std::string base_path;
    std::size_t version;
    std::shared_ptr<PreviewFiles const> files;
};

class LiveReload {
public:
    LiveReload() = default;
    LiveReload(std::filesystem::path const& output_dir, std::string base_path);

    PreviewSnapshot snapshot() const;
    void notify_build(std::filesystem::path const& output_dir, std::string base_path);
    void notify_preview(std::shared_ptr<PreviewFiles const> files, std::string base_path);
    void stream_events(httplib::Response& response);

private:
    mutable std::mutex mutex_;
    std::condition_variable changed_;
    PreviewSnapshot published_{};
};

} // namespace mizuki
