#include "cli/commands.hpp"

#include "build/builder.hpp"
#include "config/config.hpp"
#include "dev/server.hpp"
#include "dev/watch.hpp"
#include "scaffold/content.hpp"
#include "scaffold/site.hpp"
#include "util/path.hpp"

#include <spdlog/spdlog.h>

#include <charconv>
#include <exception>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>

namespace mizuki {

namespace {

constexpr int kDefaultPort = 3939;

enum class Command { Build, Watch, Serve, Init, New };
enum class ParseResult { Ok, Help, Error };

struct CliOptions {
    Command command{Command::Build};
    ConfigRequest config;
    int port{kDefaultPort};
    std::filesystem::path directory;
    NewContent content;
};

void print_usage(std::optional<Command> command = std::nullopt) {
    if (command == Command::Init) {
        std::cout << "Usage: mizuki init DIRECTORY\n"
                     "\n"
                     "Create a site with configuration, content and the default theme.\n"
                     "The directory must be absent or empty. Existing files are never replaced.\n"
                     "  --help, -h     Show this help\n";
        return;
    }
    if (command == Command::New) {
        std::cout << "Usage: mizuki new PATH --title TITLE [options]\n"
                     "\n"
                     "Create a Markdown file relative to the configured content directory.\n"
                     "  --title TEXT        Required article or page title\n"
                     "  --description TEXT  Optional single-line description\n"
                     "  --draft             Create a draft (excluded from builds and preview)\n"
                     "  --config FILE       Site configuration (default: mizuki.toml, optional)\n"
                     "  --content DIR       Override the content directory\n"
                     "  --help, -h          Show this help\n";
        return;
    }
    std::cout << "Usage: mizuki <build|watch|serve|init|new> [options]\n"
                 "\n"
                 "  init DIRECTORY Create a site in an absent or empty directory\n"
                 "  new PATH       Create a Markdown article or page (--title required)\n"
                 "  build          Build the site once\n"
                 "  watch          Rebuild when content or theme files change\n"
                 "  serve          Watch and serve the site locally\n"
                 "\n"
                 "Build, watch and serve options:\n"
                 "  --config FILE  Site configuration (default: mizuki.toml, optional)\n"
                 "  --content DIR  Content directory (default: content)\n"
                 "  --theme DIR    Theme directory (default: themes/default)\n"
                 "  --output DIR   Generated site directory (default: dist)\n"
                 "  --base-url URL Published site URL or path (default: /)\n"
                 "  --port PORT    Preview port for serve (default: 3939)\n"
                 "  --help, -h     Show help; use init --help or new --help for creation options\n";
}

std::optional<Command> parse_command(std::string_view name) {
    if (name == "init") {
        return Command::Init;
    }
    if (name == "new") {
        return Command::New;
    }
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
    return error == std::errc{} && end == value.data() + value.size() && port >= 1 && port <= 65535;
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
    bool const creating_site = cli.command == Command::Init;
    bool const creating_content = cli.command == Command::New;
    bool const building = !creating_site && !creating_content;
    bool positional_only = false;
    bool has_path = false;
    bool has_title = false;
    bool has_description = false;

    for (int index = 2; index < argc; ++index) {
        std::string_view const option(argv[index]);
        if (!positional_only && (option == "--help" || option == "-h")) {
            print_usage(cli.command);
            return ParseResult::Help;
        }
        if (!positional_only && option == "--" && !building) {
            positional_only = true;
            continue;
        }
        if (!building && (positional_only || !option.starts_with("-"))) {
            if (has_path) {
                throw std::runtime_error("Unexpected argument: " + std::string(option));
            }
            has_path = true;
            if (creating_site) {
                cli.directory = path_from_utf8(option);
            } else {
                cli.content.path = path_from_utf8(option);
            }
            continue;
        }
        if (creating_content && option == "--draft") {
            cli.content.draft = true;
            continue;
        }
        bool const is_port = cli.command == Command::Serve && option == "--port";
        bool const is_config = !creating_site && option == "--config";
        bool const is_content = !creating_site && option == "--content";
        bool const is_title = creating_content && option == "--title";
        bool const is_description = creating_content && option == "--description";
        bool const is_build_option =
            building && (option == "--theme" || option == "--output" || option == "--base-url");
        if (!is_port && !is_config && !is_content && !is_title && !is_description &&
            !is_build_option) {
            throw std::runtime_error("Unknown option: " + std::string(option));
        }
        if (index + 1 >= argc || std::string_view(argv[index + 1]).starts_with("--")) {
            throw std::runtime_error("Missing value for " + std::string(option));
        }

        std::string_view const value(argv[++index]);
        if (is_port) {
            if (!parse_port(value, cli.port)) {
                throw std::runtime_error("Invalid port: " + std::string(value) +
                                         " (expected 1-65535)");
            }
        } else if (is_config) {
            cli.config.config_file = path_from_utf8(value);
            cli.config.explicit_config = true;
        } else if (is_content) {
            cli.config.content_dir = path_from_utf8(value);
        } else if (is_title || is_description) {
            auto& seen = is_title ? has_title : has_description;
            if (seen) {
                throw std::runtime_error("Duplicate option: " + std::string(option));
            }
            seen = true;
            (is_title ? cli.content.title : cli.content.description) = value;
        } else if (option == "--base-url") {
            cli.config.base_url = value;
        } else if (option == "--theme") {
            cli.config.theme_dir = path_from_utf8(value);
        } else {
            cli.config.output_dir = path_from_utf8(value);
        }
    }
    if (!building && !has_path) {
        throw std::runtime_error(creating_site ? "Missing site directory" : "Missing content path");
    }
    if (creating_content && !has_title) {
        throw std::runtime_error("Missing required option: --title");
    }

    return ParseResult::Ok;
}

int run_build(SiteConfig const& options) {
    auto const result = build_site(options);
    spdlog::info("Built {} page(s), copied {} asset(s) -> {}", result.pages_built,
                 result.assets_copied, path_to_utf8(options.output_dir));
    return 0;
}

} // namespace

int run_cli(int argc, char** argv) {
    CliOptions cli;
    try {
        auto const result = parse_args(argc, argv, cli);
        if (result == ParseResult::Help) {
            return 0;
        }
        if (result == ParseResult::Error) {
            return 1;
        }

        switch (cli.command) {
        case Command::Init:
            init_site(cli.directory);
            spdlog::info("Created site in {}", path_to_utf8(cli.directory));
            return 0;
        case Command::New: {
            auto const path = create_content(load_config(cli.config), cli.content);
            spdlog::info("Created {}{}", path_to_utf8(path), cli.content.draft ? " (draft)" : "");
            return 0;
        }
        case Command::Serve:
            return serve(cli.config, cli.port);
        case Command::Watch:
            return watch(cli.config);
        case Command::Build:
            return run_build(load_config(cli.config));
        }
    } catch (std::exception const& error) {
        auto const action = cli.command == Command::Init  ? "Site initialization"
                            : cli.command == Command::New ? "Content creation"
                                                          : "Build";
        spdlog::error("{} failed: {}", action, error.what());
        return 1;
    }
    return 1;
}

} // namespace mizuki
