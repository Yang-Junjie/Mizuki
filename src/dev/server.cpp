#include "dev/server.hpp"

#include "build/builder.hpp"
#include "build/output_transaction.hpp"
#include "dev/live_reload.hpp"
#include "dev/preview.hpp"
#include "dev/watch.hpp"
#include "util/path.hpp"

#include <httplib.h>
#include <spdlog/spdlog.h>

#include <atomic>
#include <chrono>
#include <csignal>
#include <thread>

namespace mizuki {
namespace {

std::atomic_flag interrupted;

void interrupt(int) {
    interrupted.test_and_set(std::memory_order_relaxed);
}

} // namespace

int serve(ConfigRequest const& request, int port, bool write_output) {
    std::unique_ptr<OutputTransaction> storage;
    if (!write_output) {
        storage = std::make_unique<OutputTransaction>(std::filesystem::temp_directory_path() /
                                                      "mizuki-preview");
    }
    LiveReload reload;
    WatchSession::Build preview;
    if (storage) {
        preview = [&](SiteConfig const& config, auto const& changes) {
            auto const current = reload.snapshot();
            auto next = build_preview(config, storage->stage(), current.files, changes);
            spdlog::info("Preview built {} page(s), copied {} asset(s)", next.stats.pages_built,
                         next.stats.assets_copied);
            if (next.stats.published || current.base_path != config.base_url.path) {
                reload.notify_preview(std::move(next.files), config.base_url.path);
                if (current.version != 0) {
                    spdlog::info("Preview: http://127.0.0.1:{}{}", port, config.base_url.path);
                }
            }
        };
    }
    WatchSession session(request, std::move(preview));
    session.build_if_changed();

    auto const& initial = session.preview_config();
    auto const output_dir = initial.output_dir;
    if (!reload.snapshot().files) {
        if (!std::filesystem::is_directory(output_dir)) {
            spdlog::error("Failed to serve directory: {}", path_to_utf8(output_dir));
            return 1;
        }
        reload.notify_build(output_dir, initial.base_url.path);
    }
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
                 reload.snapshot().files ? "preview" : path_to_utf8(output_dir), port,
                 initial.base_url.path);
    std::jthread rebuild([&](std::stop_token stop) {
        session.run(stop, [&reload, port](SiteConfig const& config) {
            reload.notify_build(config.output_dir, config.base_url.path);
            spdlog::info("Preview: http://127.0.0.1:{}{}", port, config.base_url.path);
        });
    });
    interrupted.clear(std::memory_order_relaxed);
    auto const previous_signal = std::signal(SIGINT, interrupt);
    std::jthread shutdown([&](std::stop_token stop) {
        while (!stop.stop_requested()) {
            if (interrupted.test(std::memory_order_relaxed)) {
                rebuild.request_stop();
                server.stop();
                return;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    });
    auto const served = server.listen_after_bind();
    if (previous_signal != SIG_ERR) {
        std::signal(SIGINT, previous_signal);
    }
    if (!served) {
        spdlog::error("HTTP server failed on 127.0.0.1:{}", port);
        return 1;
    }
    return 0;
}

} // namespace mizuki
