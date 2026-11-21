#include "dev/server.hpp"
#include "dev/live_reload.hpp"
#include "dev/watch.hpp"

#include <httplib.h>
#include <spdlog/spdlog.h>

#include <thread>

namespace mizuki {

int serve(ConfigRequest const& request, int port) {
    LiveReload reload;
    WatchSession session(request, [&reload](BuildOptions const& options) {
        reload.notify_build(options.output_dir);
    });
    if (!session.valid()) {
        return 1;
    }
    session.build_if_changed();

    httplib::Server server;
    server.set_default_headers({{"Cache-Control", "no-store"}});
    auto const output_dir = session.options().output_dir;
    if (!std::filesystem::is_directory(output_dir)) {
        spdlog::error("Failed to serve directory: {}", output_dir.string());
        return 1;
    }
    reload.attach(server, output_dir);
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

    spdlog::info("Serving {} at http://127.0.0.1:{}. Press Ctrl+C to stop.",
                 output_dir.string(), port);
    std::jthread rebuild([&session](std::stop_token stop) { session.run(stop); });
    if (!server.listen_after_bind()) {
        spdlog::error("HTTP server failed on 127.0.0.1:{}", port);
        return 1;
    }
    return 0;
}

}  // namespace mizuki
