#pragma once

#include <condition_variable>
#include <cstddef>
#include <filesystem>
#include <mutex>

namespace httplib {
struct Response;
}

namespace mizuki {

struct PreviewSnapshot {
    std::filesystem::path output_dir;
    std::size_t version;
};

class LiveReload {
public:
    explicit LiveReload(std::filesystem::path const& output_dir);

    PreviewSnapshot snapshot() const;
    void notify_build(std::filesystem::path const& output_dir);
    void stream_events(httplib::Response& response);

private:
    mutable std::mutex mutex_;
    std::condition_variable changed_;
    std::filesystem::path output_dir_;
    std::size_t version_{0};
};

} // namespace mizuki
