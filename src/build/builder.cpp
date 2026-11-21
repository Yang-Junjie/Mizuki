#include "build/builder.hpp"

#include "build/links.hpp"
#include "build/output_transaction.hpp"
#include "build/paths.hpp"
#include "build/plan.hpp"
#include "parse/markdown.hpp"
#include "render/article_list.hpp"
#include "render/html_links.hpp"
#include "render/navigation.hpp"
#include "render/template.hpp"
#include "util/file.hpp"
#include "util/path.hpp"
#include "util/url.hpp"

#include <exception>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

namespace mizuki {
namespace fs = std::filesystem;
namespace {

std::vector<TemplateValue> site_values(SiteMetadata const& site, std::string_view navigation) {
    return {
        TemplateValue::text("{site_title}", site.title, Requirement::Optional),
        TemplateValue::text("{site_description}", site.description, Requirement::Optional),
        TemplateValue::text("{site_author}", site.author, Requirement::Optional),
        TemplateValue::text("{site_language}", site.language, Requirement::Optional),
        TemplateValue::html("{navigation}", navigation, Requirement::Optional),
    };
}

std::string navigation_for_page(SiteMetadata const& site, fs::path const& page,
                                BuildPlan const& plan) {
    auto links = site.navigation;
    for (auto& link : links) {
        link.url = resolve_link(link.url, page, plan, LinkKind::Navigation);
    }
    return render_navigation(links);
}

std::string render_page(PlannedPage const& page, Template const& layout, SiteMetadata const& site,
                        BuildPlan const& plan) {
    try {
        auto const html =
            rewrite_anchor_links(render_markdown(page.document.body), [&](std::string_view url) {
                return resolve_link(url, page.relative, plan, LinkKind::Document);
            });
        auto const asset_base =
            encode_url_path(
                path_to_utf8(fs::path("assets").lexically_relative(page.output.parent_path()))) +
            "/";
        auto const navigation = navigation_for_page(site, page.output, plan);
        auto const& metadata = page.document.metadata;
        auto values = site_values(site, navigation);
        values.push_back(TemplateValue::html("{markdown}", html));
        values.push_back(TemplateValue::text("{asset_base}", asset_base));
        values.push_back(TemplateValue::text("{title}", metadata.title));
        values.push_back(TemplateValue::text(
            "{date}", metadata.date,
            page.kind == DocumentKind::Article ? Requirement::Required : Requirement::Optional));
        return layout.render(values);
    } catch (std::exception const& error) {
        throw std::runtime_error("Failed to render " + path_to_utf8(page.source) + ": " +
                                 error.what());
    }
}

std::string render_homepage(SiteConfig const& config, BuildPlan const& plan) {
    std::vector<ArticleSummary> articles;
    for (auto const& page : plan.pages) {
        if (page.kind == DocumentKind::Article) {
            auto const& metadata = page.document.metadata;
            articles.push_back({metadata.title, metadata.date, path_to_utf8(page.output)});
        }
    }
    auto const article_list = render_article_list(std::move(articles));
    auto const navigation = navigation_for_page(config.site, "index.html", plan);
    auto values = site_values(config.site, navigation);
    values.push_back(TemplateValue::text("{asset_base}", "assets/"));
    values.push_back(TemplateValue::html("{posts}", article_list));
    return Template(config.theme_dir / "layouts/home.html").render(values);
}

} // namespace

BuildStats build_site(SiteConfig const& config) {
    auto const output_path = validate_output_path(config);
    auto const plan = plan_build(config);
    OutputTransaction output(output_path);
    std::optional<Template> article_layout;
    std::optional<Template> page_layout;
    for (auto const& page : plan.pages) {
        auto& layout = page.kind == DocumentKind::Article ? article_layout : page_layout;
        if (!layout) {
            auto const filename = page.kind == DocumentKind::Article ? "post.html" : "page.html";
            layout.emplace(config.theme_dir / "layouts" / filename);
        }
        auto const destination = output.stage() / page.output;
        fs::create_directories(destination.parent_path());
        write_file(destination, render_page(page, *layout, config.site, plan));
    }
    write_file(output.stage() / "index.html", render_homepage(config, plan));
    for (auto const& asset : plan.assets) {
        auto const destination = output.stage() / asset.output;
        fs::create_directories(destination.parent_path());
        fs::copy_file(asset.source, destination);
    }
    output.commit();
    return {plan.pages.size() + 1, plan.assets.size()};
}

} // namespace mizuki
