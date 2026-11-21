#include "dev/live_reload.hpp"

#include <httplib.h>

#include <chrono>
#include <string>

namespace mizuki {
namespace fs = std::filesystem;

LiveReload::LiveReload(fs::path const& output_dir) : output_dir_(fs::absolute(output_dir)) {}

PreviewSnapshot LiveReload::snapshot() const {
    std::lock_guard lock(mutex_);
    return {output_dir_, version_};
}

void LiveReload::notify_build(fs::path const& output_dir) {
    {
        std::lock_guard lock(mutex_);
        output_dir_ = fs::absolute(output_dir);
        ++version_;
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
                                  [&] { return version_ != version; });
            }
            auto const current = version_;
            lock.unlock();
            if (!sink.is_writable()) {
                return false;
            }
            auto const message = offset == 0 || current != version
                                     ? "data: " + std::to_string(current) + "\n\n"
                                     : std::string(": heartbeat\n\n");
            version = current;
            return sink.write(message.data(), message.size());
        });
}

} // namespace mizuki
