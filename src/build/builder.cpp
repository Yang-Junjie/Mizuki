#include "build/builder.hpp"

#include "build/context.hpp"
#include "build/links.hpp"
#include "build/output_transaction.hpp"
#include "build/paths.hpp"
#include "build/plan.hpp"
#include "config/values.hpp"
#include "parse/markdown.hpp"
#include "render/html_links.hpp"
#include "render/publication.hpp"
#include "render/theme.hpp"
#include "util/file.hpp"
#include "util/path.hpp"
#include "util/url.hpp"

#include <spdlog/spdlog.h>

#include <exception>
#include <span>
#include <stdexcept>
#include <vector>

namespace mizuki {
namespace fs = std::filesystem;
namespace {

nlohmann::json collection_context(SiteConfig const& config, BuildPlan const& plan,
                                  fs::path const& output, nlohmann::json const& extra) {
    auto context = page_context(config, plan, output, extra);
    auto const directory = output.parent_path();
    auto const path = directory.empty() ? std::string{} : path_to_utf8(directory) + "/";
    context["page"]["canonical_url"] = published_url(config.base_url, path);
    context["page"]["url"] = listing_url(output, output);
    return context;
}

std::string render_page(PlannedPage const& page, Theme& theme, SiteConfig const& config,
                        BuildPlan const& plan, nlohmann::json const& extra) {
    try {
        auto const markdown = render_markdown(page.document.body);
        auto const html =
            rewrite_document_links(markdown.html, [&](std::string_view url, HtmlLinkKind kind) {
                return resolve_link(url, page.relative, plan,
                                    kind == HtmlLinkKind::Anchor ? LinkKind::Document
                                                                 : LinkKind::Resource);
            });
        auto context = page_context(config, plan, page.output, extra);
        auto const& metadata = page.document.metadata;
        auto& data = context["page"];
        data["kind"] = page.kind == DocumentKind::Article ? "post" : "page";
        data["title"] = metadata.title;
        data["date"] = metadata.date;
        data["description"] =
            metadata.description.empty() ? config.site.description : metadata.description;
        data["content"] = html;
        data["headings"] = heading_data(markdown.headings);
        context["toc"] = toc_data(markdown.headings);
        auto const layout = metadata.layout.empty()
                                ? (page.kind == DocumentKind::Article ? "post.html" : "page.html")
                                : metadata.layout;
        return theme.render(layout, context);
    } catch (std::exception const& error) {
        throw std::runtime_error("Failed to render " + path_to_utf8(page.source) + ": " +
                                 error.what());
    }
}

std::string render_listing(SiteConfig const& config, BuildPlan const& plan, std::size_t index,
                           Theme& theme, nlohmann::json const& extra) {
    auto const& listing = plan.listings[index];
    auto const articles = std::span(plan.articles).subspan(listing.offset, listing.count);
    auto context = collection_context(config, plan, listing.output, extra);
    context["posts"] = article_data(articles, listing.output);
    context["page"]["title"] =
        index == 0 ? config.site.title
                   : config.site.title + " · 第 " + std::to_string(index + 1) + " 页";
    context["pagination"] = {
        {"current_page", index + 1},
        {"total_pages", plan.listings.size()},
        {"previous_url",
         index == 0 ? std::string{} : listing_url(plan.listings[index - 1].output, listing.output)},
        {"next_url", index + 1 == plan.listings.size()
                         ? std::string{}
                         : listing_url(plan.listings[index + 1].output, listing.output)}};
    return theme.render("home.html", context);
}

std::string render_archives(SiteConfig const& config, BuildPlan const& plan, Theme& theme,
                            nlohmann::json const& extra) {
    fs::path const output = "archives/index.html";
    auto context = collection_context(config, plan, output, extra);
    context["page"]["kind"] = "archives";
    context["page"]["title"] = "文章归档";
    auto articles = std::span(plan.articles);
    while (!articles.empty()) {
        auto const year = articles.front().date.substr(0, 4);
        std::size_t count = 0;
        while (count < articles.size() && articles[count].date.starts_with(year)) {
            ++count;
        }
        context["archives"].push_back(
            {{"year", year}, {"posts", article_data(articles.first(count), output)}});
        articles = articles.subspan(count);
    }
    return theme.render("archives.html", context);
}

} // namespace

BuildStats build_site(SiteConfig const& config) {
    auto const output_path = validate_output_path(config);
    auto const plan = plan_build(config);
    Theme theme(config.theme_dir);
    auto const extra = merge_extra(theme.defaults(), config.site.extra);
    OutputTransaction output(output_path);
    for (auto const& page : plan.pages) {
        auto const destination = output.stage() / page.output;
        fs::create_directories(destination.parent_path());
        write_file(destination, render_page(page, theme, config, plan, extra));
    }
    for (std::size_t index = 0; index < plan.listings.size(); ++index) {
        auto const destination = output.stage() / plan.listings[index].output;
        fs::create_directories(destination.parent_path());
        write_file(destination, render_listing(config, plan, index, theme, extra));
    }
    if (config.archives) {
        auto const destination = output.stage() / "archives/index.html";
        fs::create_directories(destination.parent_path());
        write_file(destination, render_archives(config, plan, theme, extra));
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
