#include "dev/live_reload.hpp"
#include "util/file.hpp"
#include "util/url.hpp"

#include <httplib.h>

#include <algorithm>
#include <chrono>
#include <cctype>
#include <string>
#include <utility>

namespace mizuki {
namespace fs = std::filesystem;

void LiveReload::notify_build(fs::path const& output_dir) {
    {
        std::lock_guard lock(mutex_);
        output_dir_ = fs::absolute(output_dir);
        ++version_;
    }
    changed_.notify_all();
}

void LiveReload::attach(httplib::Server& server, fs::path const& output_dir) {
    {
        std::lock_guard lock(mutex_);
        output_dir_ = fs::absolute(output_dir);
    }
    server.set_pre_routing_handler([this](httplib::Request const& req, httplib::Response& res) {
        if (req.method != "GET" && req.method != "HEAD") {
            return httplib::Server::HandlerResponse::Unhandled;
        }
        if (req.path == "/__mizuki/events" && req.method == "GET") {
            res.set_chunked_content_provider(
                "text/event-stream", [this, version = std::size_t{0}](std::size_t offset,
                                                                    httplib::DataSink& sink) mutable {
                    std::unique_lock lock(mutex_);
                    if (offset != 0) {
                        changed_.wait_for(lock, std::chrono::seconds(5), [&] { return version_ != version; });
                    }
                    auto const current = version_;
                    lock.unlock();
                    if (!sink.is_writable()) return false;
                    auto const message = offset == 0 || current != version
                        ? "data: " + std::to_string(current) + "\n\n" : std::string(": heartbeat\n\n");
                    version = current;
                    return sink.write(message.data(), message.size());
                });
            return httplib::Server::HandlerResponse::Handled;
        }
        fs::path root;
        std::size_t version;
        {
            std::lock_guard lock(mutex_);
            root = output_dir_;
            version = version_;
        }
        res.status = 404;
        try {
            // httplib has already URL-decoded req.path. Reject platform-specific aliases too.
            if (req.path.find('\0') != std::string::npos || req.path.find('\\') != std::string::npos) {
                return httplib::Server::HandlerResponse::Handled;
            }
            auto relative = path_from_url(req.path).relative_path();
            for (auto const& component : relative) {
                if (component == ".." || component.has_root_name() || url_path(component).find(':') != std::string::npos) {
                    return httplib::Server::HandlerResponse::Handled;
                }
            }
            auto path = root / relative;
            if (req.path.ends_with('/')) path /= "index.html";
            auto const canonical_root = fs::canonical(root);
            auto const canonical_path = fs::weakly_canonical(path);
            auto const within = canonical_path.lexically_relative(canonical_root);
            if (within.empty() || within.is_absolute() || *within.begin() == "..") {
                return httplib::Server::HandlerResponse::Handled;
            }
            if (fs::is_directory(path)) {
                res.set_redirect(encode_url_path(req.path) + "/", 301);
                return httplib::Server::HandlerResponse::Handled;
            }
            if (!fs::is_regular_file(path)) return httplib::Server::HandlerResponse::Handled;
            auto extension = url_path(path.extension());
            std::ranges::transform(extension, extension.begin(), [](unsigned char ch) {
                return static_cast<char>(std::tolower(ch));
            });
            res.status = 200;
            if (extension != ".html" && extension != ".htm") {
                res.set_file_content(url_path(path));
                return httplib::Server::HandlerResponse::Handled;
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
            res.set_content(std::move(html), "text/html; charset=utf-8");
        } catch (std::exception const&) {
            res.status = 404;
        }
        return httplib::Server::HandlerResponse::Handled;
    });
}

}  // namespace mizuki
