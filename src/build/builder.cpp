#include "build/builder.hpp"

#include "build/links.hpp"
#include "build/output_transaction.hpp"
#include "build/paths.hpp"
#include "build/plan.hpp"
#include "parse/markdown.hpp"
#include "render/article_list.hpp"
#include "render/html_links.hpp"
#include "render/navigation.hpp"
#include "render/pagination.hpp"
#include "render/publication.hpp"
#include "render/template.hpp"
#include "util/file.hpp"
#include "util/path.hpp"
#include "util/url.hpp"

#include <spdlog/spdlog.h>

#include <exception>
#include <optional>
#include <span>
#include <stdexcept>
#include <vector>

namespace mizuki {
namespace fs = std::filesystem;
namespace {

std::vector<TemplateValue> site_values(SiteMetadata const& site, std::string_view navigation,
                                       std::string_view base_url, std::string_view base_path) {
    return {
        TemplateValue::text("{site_title}", site.title, Requirement::Optional),
        TemplateValue::text("{site_description}", site.description, Requirement::Optional),
        TemplateValue::text("{site_author}", site.author, Requirement::Optional),
        TemplateValue::text("{site_language}", site.language, Requirement::Optional),
        TemplateValue::html("{navigation}", navigation, Requirement::Optional),
        TemplateValue::text("{base_url}", base_url, Requirement::Optional),
        TemplateValue::text("{base_path}", base_path, Requirement::Optional),
    };
}

void add_publication_values(std::vector<TemplateValue>& values, PublicationLinks const& links,
                            std::string_view description) {
    values.push_back(TemplateValue::text("{description}", description, Requirement::Optional));
    values.push_back(
        TemplateValue::text("{canonical_url}", links.canonical_url, Requirement::Optional));
    values.push_back(TemplateValue::text("{feed_url}", links.feed_url, Requirement::Optional));
    values.push_back(TemplateValue::html("{publication_head}", links.head, Requirement::Optional));
    values.push_back(TemplateValue::html("{feed_link}", links.feed_link, Requirement::Optional));
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
                        BuildPlan const& plan, BaseUrl const& base) {
    try {
        auto const html = rewrite_document_links(
            render_markdown(page.document.body), [&](std::string_view url, HtmlLinkKind kind) {
                return resolve_link(url, page.relative, plan,
                                    kind == HtmlLinkKind::Anchor ? LinkKind::Document
                                                                 : LinkKind::Resource);
            });
        auto const asset_base =
            encode_url_path(
                path_to_utf8(fs::path("assets").lexically_relative(page.output.parent_path()))) +
            "/";
        auto const navigation = navigation_for_page(site, page.output, plan);
        auto const& metadata = page.document.metadata;
        auto const base_url = base.origin + base.path;
        auto const publication =
            render_publication_links(base, path_to_utf8(page.output), site.title);
        auto values = site_values(site, navigation, base_url, base.path);
        add_publication_values(values, publication,
                               metadata.description.empty() ? site.description
                                                            : metadata.description);
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

std::string render_collection_page(SiteConfig const& config, BuildPlan const& plan,
                                   fs::path const& output, Template const& layout,
                                   std::span<TemplateValue const> content) {
    auto const navigation = navigation_for_page(config.site, output, plan);
    auto const base_url = config.base_url.origin + config.base_url.path;
    auto const directory = output.parent_path();
    auto const canonical_path = directory.empty() ? std::string{} : path_to_utf8(directory) + "/";
    auto const publication =
        render_publication_links(config.base_url, canonical_path, config.site.title);
    auto const asset_base = relative_url("assets/", output);
    auto values = site_values(config.site, navigation, base_url, config.base_url.path);
    add_publication_values(values, publication, config.site.description);
    values.push_back(TemplateValue::text("{asset_base}", asset_base));
    values.insert(values.end(), content.begin(), content.end());
    return layout.render(values);
}

std::string listing_url(fs::path const& target, fs::path const& source) {
    auto const directory = target.parent_path().empty() ? fs::path(".") : target.parent_path();
    return relative_url(directory / "", source);
}

std::string render_listing(SiteConfig const& config, BuildPlan const& plan, std::size_t index,
                           Template const& layout) {
    auto const& listing = plan.listings[index];
    auto const articles = std::span(plan.articles).subspan(listing.offset, listing.count);
    auto const posts = render_article_list(articles, listing.output);
    auto const previous =
        index == 0 ? std::string{} : listing_url(plan.listings[index - 1].output, listing.output);
    auto const next = index + 1 == plan.listings.size()
                          ? std::string{}
                          : listing_url(plan.listings[index + 1].output, listing.output);
    auto const pagination = render_pagination(index + 1, plan.listings.size(), previous, next);
    auto const page_number = std::to_string(index + 1);
    auto const title =
        index == 0 ? config.site.title : config.site.title + " · 第 " + page_number + " 页";
    auto const page_count = std::to_string(plan.listings.size());
    auto const requirement =
        plan.listings.size() > 1 ? Requirement::Required : Requirement::Optional;
    std::vector values{
        TemplateValue::html("{posts}", posts),
        TemplateValue::text("{title}", title, Requirement::Optional),
        TemplateValue::html("{pagination}", pagination, requirement),
        TemplateValue::text("{page_number}", page_number, Requirement::Optional),
        TemplateValue::text("{page_count}", page_count, Requirement::Optional),
    };
    return render_collection_page(config, plan, listing.output, layout, values);
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
        write_file(destination, render_page(page, *layout, config.site, plan, config.base_url));
    }
    Template const home_layout(config.theme_dir / "layouts/home.html");
    for (std::size_t index = 0; index < plan.listings.size(); ++index) {
        auto const destination = output.stage() / plan.listings[index].output;
        fs::create_directories(destination.parent_path());
        write_file(destination, render_listing(config, plan, index, home_layout));
    }
    if (config.archives) {
        fs::path const archive_path = "archives/index.html";
        Template const archive_layout(config.theme_dir / "layouts/archives.html");
        auto const archive = render_archive(plan.articles, archive_path);
        std::vector values{TemplateValue::html("{archives}", archive)};
        fs::create_directories((output.stage() / archive_path).parent_path());
        write_file(output.stage() / archive_path,
                   render_collection_page(config, plan, archive_path, archive_layout, values));
    }
    if (!config.base_url.origin.empty()) {
        write_file(output.stage() / "rss.xml",
                   render_rss(config.site, config.base_url, plan.articles));
        std::vector<std::string> urls{published_url(config.base_url, "")};
        for (std::size_t index = 1; index < plan.listings.size(); ++index) {
            auto const path = path_to_utf8(plan.listings[index].output.parent_path()) + "/";
            urls.push_back(published_url(config.base_url, path));
        }
        if (config.archives) {
            urls.push_back(published_url(config.base_url, "archives/"));
        }
        for (auto const& page : plan.pages) {
            urls.push_back(published_url(config.base_url, path_to_utf8(page.output)));
        }
        write_file(output.stage() / "sitemap.xml", render_sitemap(urls));
    } else {
        spdlog::info("Skipping RSS, sitemap and canonical URLs: set base_url to a complete HTTP(S) "
                     "address to enable them");
    }
    for (auto const& asset : plan.assets) {
        auto const destination = output.stage() / asset.output;
        fs::create_directories(destination.parent_path());
        fs::copy_file(asset.source, destination);
    }
    output.commit();
    return {plan.pages.size() + plan.listings.size() + (config.archives ? 1 : 0),
            plan.assets.size()};
}

} // namespace mizuki
