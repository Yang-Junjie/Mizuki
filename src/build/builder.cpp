#include "build/builder.hpp"
#include "parse/markdown.hpp"
#include "render/renderer.hpp"
#include "util/file.hpp"
#include "util/path.hpp"

#include <exception>
#include <utility>

namespace mizuki {

namespace fs = std::filesystem;

Builder::Builder(BuildOptions options) : options_(std::move(options)) {}

BuildResult Builder::build() const {
    BuildResult result;
    try {
        fs::create_directories(options_.output_dir);
        Renderer renderer(options_.theme_dir / "layouts/post.html");

        // Render Markdown pages and copy other content files.
        for (auto const& file : fs::recursive_directory_iterator(options_.content_dir)) {
            if (!file.is_regular_file()) {
                continue;
            }

            auto const relative = file.path().lexically_relative(options_.content_dir);
            auto destination = options_.output_dir / relative;
            fs::create_directories(destination.parent_path());

            if (is_markdown(file.path())) {
                destination.replace_extension(".html");
                Markdown markdown(file.path());
                auto const asset_base =
                    fs::path("assets").lexically_relative(relative.parent_path());
                if (!markdown.to_html() ||
                    !renderer.render(markdown.get_html(), asset_base.generic_string() + "/")) {
                    result.error = "Failed to render " + file.path().string();
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
                auto const destination = options_.output_dir / "assets" / relative;
                fs::create_directories(destination.parent_path());
                fs::copy_file(file.path(), destination, fs::copy_options::overwrite_existing);
                ++result.assets_copied;
            }
        }
        result.success = true;
    } catch (std::exception const& error) {
        result.error = error.what();
    }
    return result;
}

}  // namespace mizuki
