#include "dev/server.hpp"

#include "dev/live_reload.hpp"
#include "dev/preview.hpp"
#include "dev/watch.hpp"
#include "util/path.hpp"

#include <httplib.h>
#include <spdlog/spdlog.h>

#include <thread>

namespace mizuki {

int serve(ConfigRequest const& request, int port) {
    WatchSession session(request);
    session.build_if_changed();

    auto const& initial = session.preview_config();
    auto const output_dir = initial.output_dir;
    if (!std::filesystem::is_directory(output_dir)) {
        spdlog::error("Failed to serve directory: {}", path_to_utf8(output_dir));
        return 1;
    }
    LiveReload reload(output_dir, initial.base_url.path);
    httplib::Server server;
    server.set_default_headers({{"Cache-Control", "no-store"}});
    attach_preview(server, reload);
    // Replace the library's default SO_REUSEPORT so another server cannot share the port.
    server.set_socket_options([](socket_t socket) {
#ifdef _WIN32
        httplib::set_socket_opt(socket, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, 1);
#else
        httplib::set_socket_opt(socket, SOL_SOCKET, SO_REUSEADDR, 1);
#endif
    });
    if (!server.bind_to_port("127.0.0.1", port)) {
        spdlog::error("Failed to bind 127.0.0.1:{}; the port may be in use", port);
        return 1;
    }

    spdlog::info("Serving {} at http://127.0.0.1:{}{}. Press Ctrl+C to stop.",
                 path_to_utf8(output_dir), port, initial.base_url.path);
    std::jthread rebuild([&](std::stop_token stop) {
        session.run(stop, [&reload, port](SiteConfig const& config) {
            reload.notify_build(config.output_dir, config.base_url.path);
            spdlog::info("Preview: http://127.0.0.1:{}{}", port, config.base_url.path);
        });
    });
    if (!server.listen_after_bind()) {
        spdlog::error("HTTP server failed on 127.0.0.1:{}", port);
        return 1;
    }
    return 0;
}

} // namespace mizuki
