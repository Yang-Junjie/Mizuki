#include "dev/preview.hpp"

#include "dev/live_reload.hpp"
#include "util/file.hpp"
#include "util/path.hpp"
#include "util/url.hpp"

#include <httplib.h>

#include <algorithm>
#include <cctype>
#include <exception>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace mizuki {
namespace fs = std::filesystem;
namespace {

std::optional<fs::path> resolve_request_path(fs::path const& root, std::string_view url) {
    // httplib has already URL-decoded the path. Reject platform-specific aliases too.
    if (url.find('\0') != std::string_view::npos || url.find('\\') != std::string_view::npos) {
        return std::nullopt;
    }
    auto const relative = path_from_utf8(url).relative_path();
    for (auto const& component : relative) {
        if (component == ".." || component.has_root_name() ||
            path_to_utf8(component).find(':') != std::string::npos) {
            return std::nullopt;
        }
    }
    auto path = root / relative;
    if (url.ends_with('/')) {
        path /= "index.html";
    }
    auto const within = fs::weakly_canonical(path).lexically_relative(fs::canonical(root));
    if (within.empty() || within.is_absolute() || *within.begin() == "..") {
        return std::nullopt;
    }
    return path;
}

std::string inject_reload_script(std::string html, std::string_view local_path,
                                 PreviewSnapshot const& snapshot) {
    std::string script = R"(
<script>
(() => {
    const version = ")";
    script += std::to_string(snapshot.version);
    script += R"(";
    const page = ")";
    script += encode_url_path(local_path.substr(1));
    script += R"(";
    const events = new EventSource(")";
    script += snapshot.base_path + "__mizuki/events";
    script += R"(");
    events.onmessage = (event) => {
        const published = JSON.parse(event.data);
        if (String(published.version) !== version) {
            events.close();
            location.replace(published.base_path + page + location.search + location.hash);
        }
    };
})();
</script>
)";
    auto const position = html.rfind("</body>");
    html.insert(position == std::string::npos ? html.size() : position, script);
    return html;
}

void serve_file(httplib::Request const& request, std::string_view local_path,
                httplib::Response& response, PreviewSnapshot const& snapshot) {
    response.status = 404;
    auto const path = resolve_request_path(snapshot.output_dir, local_path);
    if (!path) {
        return;
    }
    if (fs::is_directory(*path)) {
        auto const query = request.target.find('?');
        response.set_redirect(encode_url_path(request.path) + "/" +
                                  (query == std::string::npos ? "" : request.target.substr(query)),
                              301);
        return;
    }
    if (!fs::is_regular_file(*path)) {
        return;
    }
    auto extension = path_to_utf8(path->extension());
    std::ranges::transform(extension, extension.begin(),
                           [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    response.status = 200;
    if (extension == ".html" || extension == ".htm") {
        response.set_content(inject_reload_script(read_file(*path), local_path, snapshot),
                             "text/html; charset=utf-8");
    } else {
        response.set_file_content(path_to_utf8(*path));
    }
}

} // namespace

void attach_preview(httplib::Server& server, LiveReload& reload) {
    server.set_pre_routing_handler(
        [&reload](httplib::Request const& request, httplib::Response& response) {
            if (request.method != "GET" && request.method != "HEAD") {
                return httplib::Server::HandlerResponse::Unhandled;
            }
            auto const snapshot = reload.snapshot();
            auto const mount = decode_url_path(snapshot.base_path);
            auto const query = request.target.find('?');
            if (mount != "/" && request.path == mount.substr(0, mount.size() - 1)) {
                response.set_redirect(snapshot.base_path + (query == std::string::npos
                                                                ? ""
                                                                : request.target.substr(query)),
                                      301);
            } else if (!request.path.starts_with(mount)) {
                response.status = 404;
            } else {
                auto const local_path = "/" + request.path.substr(mount.size());
                if (local_path.starts_with("//")) {
                    response.status = 404;
                } else if (local_path == "/__mizuki/events" && request.method == "GET") {
                    reload.stream_events(response);
                } else {
                    try {
                        serve_file(request, local_path, response, snapshot);
                    } catch (std::exception const&) {
                        response.status = 404;
                    }
                }
            }
            return httplib::Server::HandlerResponse::Handled;
        });
}

} // namespace mizuki
