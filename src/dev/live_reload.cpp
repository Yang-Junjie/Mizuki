#include "dev/live_reload.hpp"

#include <httplib.h>

#include <chrono>
#include <string>
#include <utility>

namespace mizuki {
namespace fs = std::filesystem;

LiveReload::LiveReload(fs::path const& output_dir, std::string base_path)
    : published_{fs::absolute(output_dir), std::move(base_path), 0, {}} {}

PreviewSnapshot LiveReload::snapshot() const {
    std::lock_guard lock(mutex_);
    return published_;
}

void LiveReload::notify_build(fs::path const& output_dir, std::string base_path) {
    {
        std::lock_guard lock(mutex_);
        published_ = {fs::absolute(output_dir), std::move(base_path), published_.version + 1, {}};
    }
    changed_.notify_all();
}

void LiveReload::notify_preview(std::shared_ptr<PreviewFiles const> files, std::string base_path) {
    {
        std::lock_guard lock(mutex_);
        published_ = {{}, std::move(base_path), published_.version + 1, std::move(files)};
    }
    changed_.notify_all();
}

void LiveReload::stream_events(httplib::Response& response) {
    response.set_chunked_content_provider(
        "text/event-stream",
        [this, version = std::size_t{0}](std::size_t offset, httplib::DataSink& sink) mutable {
            std::unique_lock lock(mutex_);
            if (offset != 0) {
                changed_.wait_for(lock, std::chrono::seconds(5),
                                  [&] { return published_.version != version; });
            }
            auto const current = published_;
            lock.unlock();
            if (!sink.is_writable()) {
                return false;
            }
            // base_path is normalized URL-encoded ASCII, so it needs no JSON escaping.
            auto const message = offset == 0 || current.version != version
                                     ? "data: {\"version\":" + std::to_string(current.version) +
                                           ",\"base_path\":\"" + current.base_path + "\"}\n\n"
                                     : std::string(": heartbeat\n\n");
            version = current.version;
            return sink.write(message.data(), message.size());
        });
}

} // namespace mizuki
