#include "build/builder.hpp"
#include "build/article_list.hpp"
#include "build/output_transaction.hpp"
#include "build/paths.hpp"
#include "parse/markdown.hpp"
#include "render/renderer.hpp"
#include "util/file.hpp"
#include "util/path.hpp"

#include <spdlog/spdlog.h>

#include <array>
#include <exception>
#include <utility>
#include <vector>

namespace mizuki {

namespace fs = std::filesystem;

Builder::Builder(BuildOptions options) : options_(std::move(options)) {}

BuildResult Builder::build() const {
    BuildResult result;
    try {
        OutputTransaction output(validate_output_path(options_));
        Renderer post_renderer(options_.theme_dir / "layouts/post.html");
        Renderer home_renderer(options_.theme_dir / "layouts/home.html");
        std::vector<ArticleSummary> articles;

        // Render Markdown pages and copy other content files.
        for (auto const& file : fs::recursive_directory_iterator(options_.content_dir)) {
            if (!file.is_regular_file()) {
                continue;
            }

            auto const relative = file.path().lexically_relative(options_.content_dir);
            if (relative == fs::path("index.md")) {
                continue;
            }

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
                if (!markdown.to_html()) {
                    result.error = "Failed to render " + file.path().string();
                    if (!markdown.error().empty()) {
                        result.error += ": " + markdown.error();
                    }
                    return result;
                }

                std::array<TemplateValue, 4> const values{{
                    {"{markdown}", markdown.get_html(), true},
                    {"{asset_base}", asset_base_path},
                    {"{title}", markdown.front_matter().title},
                    {"{date}", markdown.front_matter().date},
                }};
                if (!post_renderer.render(values)) {
                    result.error = "Failed to render " + file.path().string();
                    return result;
                }
                write_file(destination, post_renderer.get_html());

                auto article_url = relative;
                article_url.replace_extension(".html");
                articles.push_back({markdown.front_matter().title, markdown.front_matter().date,
                                    article_url.generic_string()});
                ++result.pages_built;
            } else {
                fs::copy_file(file.path(), destination, fs::copy_options::overwrite_existing);
                ++result.assets_copied;
            }
        }

        auto const article_list = render_article_list(std::move(articles));
        std::array<TemplateValue, 2> const home_values{{
            {"{asset_base}", "assets/"},
            {"{posts}", article_list, true},
        }};
        if (!home_renderer.render(home_values)) {
            result.error = "Failed to render homepage template";
            return result;
        }
        write_file(output.stage() / "index.html", home_renderer.get_html());
        ++result.pages_built;

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
