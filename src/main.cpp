#include "build/builder.hpp"
#include "dev/server.hpp"
#include "dev/watch.hpp"

#include <spdlog/spdlog.h>

#include <charconv>
#include <iostream>
#include <string_view>

namespace {

void print_usage() {
    std::cout << "Usage: mizuki <build|watch|serve> [options]\n"
                 "\n"
                 "  build          Build the site once\n"
                 "  watch          Rebuild when content or theme files change\n"
                 "  serve          Watch and serve the site locally\n"
                 "\n"
                 "  --content DIR  Content directory (default: content)\n"
                 "  --theme DIR    Theme directory (default: themes/default)\n"
                 "  --output DIR   Generated site directory (default: dist)\n"
                 "  --port PORT    Preview port for serve (default: 3939)\n"
                 "  --help, -h     Show this help\n";
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        print_usage();
        return 1;
    }
    std::string_view const command(argv[1]);
    if (command == "--help" || command == "-h") {
        print_usage();
        return 0;
    }
    if (command != "build" && command != "watch" && command != "serve") {
        spdlog::error("Unknown command: {}", command);
        print_usage();
        return 1;
    }

    mizuki::BuildOptions options;
    int port = 3939;
    for (int index = 2; index < argc; ++index) {
        std::string_view const option(argv[index]);
        if (option == "--help" || option == "-h") {
            print_usage();
            return 0;
        }
        bool const is_port = command == "serve" && option == "--port";
        if (option != "--content" && option != "--theme" && option != "--output" && !is_port) {
            spdlog::error("Unknown option: {}", option);
            return 1;
        }
        if (index + 1 >= argc || std::string_view(argv[index + 1]).starts_with("--")) {
            spdlog::error("Missing value for {}", option);
            return 1;
        }
        std::string_view const value(argv[++index]);
        if (is_port) {
            auto const [end, error] =
                std::from_chars(value.data(), value.data() + value.size(), port);
            if (error != std::errc{} || end != value.data() + value.size() || port < 1 ||
                port > 65535) {
                spdlog::error("Invalid port: {} (expected 1-65535)", value);
                return 1;
            }
        } else if (option == "--content") {
            options.content_dir = value;
        } else if (option == "--theme") {
            options.theme_dir = value;
        } else {
            options.output_dir = value;
        }
    }

    if (command == "serve") {
        return mizuki::serve(options, port);
    }
    if (command == "watch") {
        return mizuki::watch(options);
    }

    mizuki::Builder builder(options);
    auto const result = builder.build();
    if (!result.success) {
        spdlog::error("Build failed: {}", result.error);
        return 1;
    }
    spdlog::info("Built {} page(s), copied {} asset(s) -> {}", result.pages_built,
                 result.assets_copied, options.output_dir.string());
    return 0;
}
