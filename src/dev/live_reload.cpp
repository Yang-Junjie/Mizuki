#include "dev/live_reload.hpp"
#include "util/file.hpp"

#include <httplib.h>

#include <chrono>
#include <string>
#include <utility>

namespace mizuki {

void LiveReload::notify_build() {
    {
        std::lock_guard lock(mutex_);
        ++version_;
    }
    changed_.notify_all();
}

void LiveReload::attach(httplib::Server& server, std::filesystem::path const& output_dir) {
    // Internal requests must run before the mounted static file handler.
    server.set_pre_routing_handler([this](httplib::Request const& req, httplib::Response& res) {
        if (req.method != "GET" || req.path != "/__mizuki/events") {
            return httplib::Server::HandlerResponse::Unhandled;
        }
        res.set_chunked_content_provider(
            "text/event-stream", [this, version = std::size_t{0}](std::size_t offset,
                                                                httplib::DataSink& sink) mutable {
                std::unique_lock lock(mutex_);
                // Send the current version immediately on every new connection.
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
        return httplib::Server::HandlerResponse::Handled;
    });

    server.set_file_request_handler(
        [this, output_dir](httplib::Request const& req, httplib::Response& res) {
            auto const content_type = res.get_header_value("Content-Type");
            if (!content_type.starts_with("text/html")) {
                return;
            }
            // Read the version first so a build completed during the read triggers a reload.
            std::size_t version;
            {
                std::lock_guard lock(mutex_);
                version = version_;
            }

            // The static file handler has already checked this path.
            auto path = output_dir / std::filesystem::path(req.path).relative_path();
            if (req.path.back() == '/') {
                path /= "index.html";
            }
            auto html = read_file(path);
            auto const script = R"(
<script>
(() => {
    const version = ")" + std::to_string(version) + R"(";
    const events = new EventSource("/__mizuki/events");
    events.onmessage = (event) => {
        if (event.data !== version) {
            events.close();
            location.reload();
        }
    };
})();
</script>
)";
            auto const position = html.rfind("</body>");
            html.insert(position == std::string::npos ? html.size() : position, script);
            res.set_content(std::move(html), content_type);
        });
}

}  // namespace mizuki
