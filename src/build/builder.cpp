#include "build/builder.hpp"
#include "build/article_list.hpp"
#include "build/links.hpp"
#include "build/output_transaction.hpp"
#include "build/paths.hpp"
#include "build/plan.hpp"
#include "render/renderer.hpp"
#include "util/file.hpp"
#include "util/url.hpp"

#include <exception>
#include <stdexcept>
#include <utility>
#include <vector>

namespace mizuki {
namespace fs = std::filesystem;

Builder::Builder(BuildOptions options) : options_(std::move(options)) {}

BuildResult Builder::build() const {
    BuildResult result;
    try {
        auto const output_path = validate_output_path(options_);
        auto plan = plan_build(options_);
        OutputTransaction output(output_path);
        Renderer post_renderer(options_.theme_dir / "layouts/post.html");
        Renderer page_renderer(options_.theme_dir / "layouts/page.html");
        Renderer home_renderer(options_.theme_dir / "layouts/home.html");
        std::vector<ArticleSummary> articles;
        auto add_site_values = [&](std::vector<TemplateValue>& values, std::string const& navigation) {
            values.push_back({"{site_title}", options_.site.title, false, false});
            values.push_back({"{site_description}", options_.site.description, false, false});
            values.push_back({"{site_author}", options_.site.author, false, false});
            values.push_back({"{site_language}", options_.site.language, false, false});
            values.push_back({"{navigation}", navigation, true, false});
        };
        for (auto& page : plan.pages) {
            if (!page.markdown.to_html()) {
                throw std::runtime_error("Failed to render " + url_path(page.source) + ": " + page.markdown.error());
            }
            auto const html = rewrite_document_links(page.markdown.get_html(), page.relative, plan);
            auto const asset_base = encode_url_path(url_path(
                fs::path("assets").lexically_relative(page.output.parent_path()))) + "/";
            auto const navigation = render_navigation(options_.site, page.output, plan);
            auto const& metadata = page.markdown.front_matter();
            std::vector<TemplateValue> values{
                {"{markdown}", html, true},
                {"{asset_base}", asset_base},
                {"{title}", metadata.title},
                {"{date}", metadata.date, false, !page.independent},
            };
            add_site_values(values, navigation);
            auto& renderer = page.independent ? page_renderer : post_renderer;
            if (!renderer.render(values)) {
                auto const layout = page.independent ? "layouts/page.html" : "layouts/post.html";
                throw std::runtime_error("Failed to render " + url_path(page.source) +
                                         "; check template " + url_path(options_.theme_dir / layout));
            }
            auto const destination = output.stage() / page.output;
            fs::create_directories(destination.parent_path());
            write_file(destination, renderer.get_html());
            if (!page.independent) {
                articles.push_back({metadata.title, metadata.date, url_path(page.output)});
            }
            ++result.pages_built;
        }
        auto const article_list = render_article_list(std::move(articles));
        auto const navigation = render_navigation(options_.site, "index.html", plan);
        std::vector<TemplateValue> home_values{{"{asset_base}", "assets/"}, {"{posts}", article_list, true}};
        add_site_values(home_values, navigation);
        if (!home_renderer.render(home_values)) {
            throw std::runtime_error("Failed to render homepage template: " +
                                     url_path(options_.theme_dir / "layouts/home.html"));
        }
        write_file(output.stage() / "index.html", home_renderer.get_html());
        ++result.pages_built;
        for (auto const& asset : plan.assets) {
            auto const destination = output.stage() / asset.output;
            fs::create_directories(destination.parent_path());
            fs::copy_file(asset.source, destination);
            ++result.assets_copied;
        }
        output.commit();
        result.success = true;
    } catch (std::exception const& error) {
        result.error = error.what();
    }
    return result;
}

}  // namespace mizuki
