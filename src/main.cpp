#include "parse/markdown.hpp"
#include "render/renderer.hpp"

#include <spdlog/spdlog.h>

#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <system_error>

namespace {

void copy_asset(std::filesystem::path const& from, std::filesystem::path const& to) {
    std::error_code ec;
    std::filesystem::copy_file(from, to, std::filesystem::copy_options::overwrite_existing, ec);
    if (ec) {
        throw std::runtime_error("Failed to copy " + from.string() + " to " + to.string() + ": " +
                                 ec.message());
    }
}

}  // namespace

int main(int argc, char** args) {

    if (argc < 2) {
        spdlog::error("No arguments provided");
        return 1;
    }

    std::filesystem::path markdown_path(args[1]);
    std::filesystem::path const theme_root("themes/default");

    mizuki::Markdown markdown(markdown_path);
    if (!markdown.to_html()) {
        spdlog::error("Failed to convert markdown: {}", markdown_path.string());
        return 1;
    }

    mizuki::Renderer renderer(theme_root / "layouts/post.html");
    if (!renderer.render(markdown.get_html())) {
        spdlog::error("Failed to render page for: {}", markdown_path.string());
        return 1;
    }

    std::string const& page = renderer.get_html();

    try {
        std::ofstream html_file("output.html", std::ios::binary);
        if (!html_file.is_open()) {
            spdlog::error("Failed to open HTML output file");
            return 1;
        }
        html_file << page;
        html_file.close();

        copy_asset(theme_root / "assets/css/style.css", "style.css");
    } catch (std::exception const& e) {
        spdlog::error("{}", e.what());
        return 1;
    }

    spdlog::info("Rendered {} -> output.html", markdown_path.string());
    return 0;
}
