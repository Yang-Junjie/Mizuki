#include "cli/commands.hpp"
#include "build/builder.hpp"
#include "config/config.hpp"
#include "dev/server.hpp"
#include "dev/watch.hpp"

#include <spdlog/spdlog.h>

#include <charconv>
#include <iostream>
#include <optional>
#include <string_view>

namespace mizuki {

namespace {

constexpr int kDefaultPort = 3939;

enum class Command { Build, Watch, Serve };
enum class ParseResult { Ok, Help, Error };

struct CliOptions {
    Command command{Command::Build};
    ConfigRequest config;
    int port{kDefaultPort};
};

void print_usage() {
    std::cout << "Usage: mizuki <build|watch|serve> [options]\n"
                 "\n"
                 "  build          Build the site once\n"
                 "  watch          Rebuild when content or theme files change\n"
                 "  serve          Watch and serve the site locally\n"
                 "\n"
                 "  --config FILE  Site configuration (default: mizuki.toml, optional)\n"
                 "  --content DIR  Content directory (default: content)\n"
                 "  --theme DIR    Theme directory (default: themes/default)\n"
                 "  --output DIR   Generated site directory (default: dist)\n"
                 "  --port PORT    Preview port for serve (default: 3939)\n"
                 "  --help, -h     Show this help\n";
}

std::optional<Command> parse_command(std::string_view name) {
    if (name == "build") {
        return Command::Build;
    }
    if (name == "watch") {
        return Command::Watch;
    }
    if (name == "serve") {
        return Command::Serve;
    }
    return std::nullopt;
}

bool parse_port(std::string_view value, int& port) {
    auto const [end, error] = std::from_chars(value.data(), value.data() + value.size(), port);
    return error == std::errc{} && end == value.data() + value.size() && port >= 1 &&
           port <= 65535;
}

ParseResult parse_args(int argc, char** argv, CliOptions& cli) {
    if (argc < 2) {
        print_usage();
        return ParseResult::Error;
    }

    std::string_view const command_name(argv[1]);
    if (command_name == "--help" || command_name == "-h") {
        print_usage();
        return ParseResult::Help;
    }

    auto const command = parse_command(command_name);
    if (!command) {
        spdlog::error("Unknown command: {}", command_name);
        print_usage();
        return ParseResult::Error;
    }
    cli.command = *command;
    bool const accepts_port = cli.command == Command::Serve;

    for (int index = 2; index < argc; ++index) {
        std::string_view const option(argv[index]);
        if (option == "--help" || option == "-h") {
            print_usage();
            return ParseResult::Help;
        }

        bool const is_port = accepts_port && option == "--port";
        if (option != "--content" && option != "--theme" && option != "--output" &&
            option != "--config" && !is_port) {
            spdlog::error("Unknown option: {}", option);
            return ParseResult::Error;
        }
        if (index + 1 >= argc || std::string_view(argv[index + 1]).starts_with("--")) {
            spdlog::error("Missing value for {}", option);
            return ParseResult::Error;
        }

        std::string_view const value(argv[++index]);
        if (is_port) {
            if (!parse_port(value, cli.port)) {
                spdlog::error("Invalid port: {} (expected 1-65535)", value);
                return ParseResult::Error;
            }
        } else if (option == "--config") {
            cli.config.config_file = value;
            cli.config.explicit_config = true;
        } else if (option == "--content") {
            cli.config.content_dir = std::filesystem::path(value);
        } else if (option == "--theme") {
            cli.config.theme_dir = std::filesystem::path(value);
        } else {
            cli.config.output_dir = std::filesystem::path(value);
        }
    }

    return ParseResult::Ok;
}

int run_build(BuildOptions const& options) {
    Builder builder(options);
    auto const result = builder.build();
    if (!result.success) {
        spdlog::error("Build failed: {}", result.error);
        return 1;
    }
    spdlog::info("Built {} page(s), copied {} asset(s) -> {}", result.pages_built,
                 result.assets_copied, options.output_dir.string());
    return 0;
}

}  // namespace

int run_cli(int argc, char** argv) {
    CliOptions cli;
    auto const result = parse_args(argc, argv, cli);
    if (result == ParseResult::Help) {
        return 0;
    }
    if (result == ParseResult::Error) {
        return 1;
    }

    try {
        switch (cli.command) {
            case Command::Serve:
                return serve(cli.config, cli.port);
            case Command::Watch:
                return watch(cli.config);
            case Command::Build:
                return run_build(load_config(cli.config));
        }
    } catch (std::exception const& error) {
        spdlog::error("{}", error.what());
        return 1;
    }
    return 1;
}

}  // namespace mizuki
