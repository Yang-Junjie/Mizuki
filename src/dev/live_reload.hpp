#pragma once

#include <condition_variable>
#include <cstddef>
#include <filesystem>
#include <mutex>
#include <string>

namespace httplib {
struct Response;
}

namespace mizuki {

struct PreviewSnapshot {
    std::filesystem::path output_dir;
    std::string base_path;
    std::size_t version;
};

class LiveReload {
public:
    LiveReload(std::filesystem::path const& output_dir, std::string base_path);

    PreviewSnapshot snapshot() const;
    void notify_build(std::filesystem::path const& output_dir, std::string base_path);
    void stream_events(httplib::Response& response);

private:
    mutable std::mutex mutex_;
    std::condition_variable changed_;
    PreviewSnapshot published_;
};

} // namespace mizuki
