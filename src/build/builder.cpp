#include "build/builder.hpp"
#include "build/output_transaction.hpp"
#include "build/paths.hpp"
#include "parse/markdown.hpp"
#include "render/renderer.hpp"
#include "util/file.hpp"
#include "util/path.hpp"

#include <spdlog/spdlog.h>

#include <exception>
#include <utility>

namespace mizuki {

namespace fs = std::filesystem;

Builder::Builder(BuildOptions options) : options_(std::move(options)) {}

BuildResult Builder::build() const {
    BuildResult result;
    try {
        OutputTransaction output(validate_output_path(options_));
        Renderer renderer(options_.theme_dir / "layouts/post.html");

        // Render Markdown pages and copy other content files.
        for (auto const& file : fs::recursive_directory_iterator(options_.content_dir)) {
            if (!file.is_regular_file()) {
                continue;
            }

            auto const relative = file.path().lexically_relative(options_.content_dir);
            auto destination = output.stage() / relative;
            fs::create_directories(destination.parent_path());

            if (is_markdown(file.path())) {
                destination.replace_extension(".html");
                Markdown markdown(file.path());
                if (!markdown.load()) {
                    result.error = "Failed to render " + file.path().string() + ": " + markdown.error();
                    return result;
                }
                if (markdown.front_matter().draft) {
                    spdlog::info("Skipping draft: {}", file.path().string());
                    continue;
                }
                auto const asset_base = fs::path("assets").lexically_relative(relative.parent_path());
                std::string const asset_base_path = asset_base.generic_string() + "/";
                RenderValues const values{asset_base_path, markdown.front_matter().title, markdown.front_matter().date};
                if (!markdown.to_html() || !renderer.render(markdown.get_html(), values)) {
                    result.error = "Failed to render " + file.path().string();
                    if (!markdown.error().empty()) {
                        result.error += ": " + markdown.error();
                    }
                    return result;
                }
                write_file(destination, renderer.get_html());
                ++result.pages_built;
            } else {
                fs::copy_file(file.path(), destination, fs::copy_options::overwrite_existing);
                ++result.assets_copied;
            }
        }

        // Copy the theme's static assets into output/assets.
        auto const theme_assets = options_.theme_dir / "assets";
        if (fs::exists(theme_assets)) {
            for (auto const& file : fs::recursive_directory_iterator(theme_assets)) {
                if (!file.is_regular_file()) {
                    continue;
                }

                auto const relative = file.path().lexically_relative(theme_assets);
                auto const destination = output.stage() / "assets" / relative;
                fs::create_directories(destination.parent_path());
                fs::copy_file(file.path(), destination, fs::copy_options::overwrite_existing);
                ++result.assets_copied;
            }
        }
        output.commit();
        result.success = true;
    } catch (std::exception const& error) {
        result.error = error.what();
    }
    return result;
}

}  // namespace mizuki
