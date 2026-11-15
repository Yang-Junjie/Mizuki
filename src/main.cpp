#include "build/builder.hpp"
#include "dev/watch.hpp"

#include <spdlog/spdlog.h>

#include <iostream>
#include <string_view>

namespace {

void print_usage() {
    std::cout << "Usage: mizuki <build|watch> [--content DIR] [--theme DIR] [--output DIR]\n"
                 "\n"
                 "  build          Build the site once\n"
                 "  watch          Rebuild when content or theme files change\n"
                 "\n"
                 "  --content DIR  Content directory (default: content)\n"
                 "  --theme DIR    Theme directory (default: themes/default)\n"
                 "  --output DIR   Generated site directory (default: dist)\n"
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
    if (command != "build" && command != "watch") {
        spdlog::error("Unknown command: {}", command);
        print_usage();
        return 1;
    }

    mizuki::BuildOptions options;
    for (int index = 2; index < argc; ++index) {
        std::string_view const option(argv[index]);
        if (option == "--help" || option == "-h") {
            print_usage();
            return 0;
        }
        if (option != "--content" && option != "--theme" && option != "--output") {
            spdlog::error("Unknown option: {}", option);
            return 1;
        }
        if (index + 1 >= argc || std::string_view(argv[index + 1]).starts_with("--")) {
            spdlog::error("Missing directory value for {}", option);
            return 1;
        }
        auto const* value = argv[++index];
        if (option == "--content") {
            options.content_dir = value;
        } else if (option == "--theme") {
            options.theme_dir = value;
        } else {
            options.output_dir = value;
        }
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
