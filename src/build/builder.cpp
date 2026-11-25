#include "build/builder.hpp"

#include "build/context.hpp"
#include "build/images.hpp"
#include "build/incremental.hpp"
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
#include <map>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>

namespace mizuki {
namespace fs = std::filesystem;
namespace {

using WordCounts = std::map<std::string, std::size_t>;

nlohmann::json collection_context(SiteConfig const& config, BuildPlan const& plan,
                                  fs::path const& output, nlohmann::json const& extra) {
    auto context = page_context(config, plan, output, extra);
    auto const directory = output.parent_path();
    auto const path = directory.empty() ? std::string{} : path_to_utf8(directory) + "/";
    context["page"]["canonical_url"] = published_url(config.base_url, path);
    context["page"]["url"] = listing_url(output, output);
    return context;
}

std::string render_page(PlannedPage const& page, Theme& theme, RenderCache& cache,
                        SiteConfig const& config, BuildPlan const& plan,
                        nlohmann::json const& extra, WordCounts& word_counts) {
    try {
        auto const markdown = cache.markdown(page.document.body, config.markdown);
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
        data["extra"] = metadata.extra;
        data["tags"] = term_links(metadata.tags, "tags", page.output);
        data["categories"] = term_links(metadata.categories, "categories", page.output);
        data["description"] =
            metadata.description.empty() ? config.site.description : metadata.description;
        data["content"] = html;
        if (page.kind == DocumentKind::Article) {
            data["word_count"] = markdown.word_count;
            word_counts.emplace(path_to_utf8(page.output), markdown.word_count);
        }
        data["headings"] = heading_data(markdown.headings);
        data["features"] = {{"code", markdown.features.code},
                            {"math", markdown.features.math},
                            {"footnotes", markdown.features.footnotes},
                            {"admonitions", markdown.features.admonitions}};
        data["code_languages"] = markdown.code_languages;
        context["toc"] = toc_data(markdown.headings);
        auto const layout = metadata.layout.empty()
                                ? (page.kind == DocumentKind::Article ? "post.html" : "page.html")
                                : metadata.layout;
        return cache.page(theme, layout, context);
    } catch (std::exception const& error) {
        throw std::runtime_error("Failed to render " + path_to_utf8(page.source) + ": " +
                                 error.what());
    }
}

std::string render_listing(SiteConfig const& config, BuildPlan const& plan, std::size_t index,
                           Theme& theme, RenderCache& cache, nlohmann::json const& extra,
                           WordCounts const& word_counts) {
    auto const& listing = plan.listings[index];
    auto const articles = std::span(plan.articles).subspan(listing.offset, listing.count);
    auto context = collection_context(config, plan, listing.output, extra);
    for (auto const& article : articles) {
        auto post = summary_value(article, listing.output);
        post["word_count"] = word_counts.at(article.url);
        context["posts"].push_back(std::move(post));
    }
    context["page"]["title"] =
        index == 0 ? config.site.title
                   : config.site.title + " · 第 " + std::to_string(index + 1) + " 页";
    context["pagination"] = pagination_data(plan.listings, index);
    return cache.page(theme, "home.html", context);
}

std::string render_archives(SiteConfig const& config, BuildPlan const& plan, Theme& theme,
                            RenderCache& cache, nlohmann::json const& extra) {
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
            {{"year", year}, {"posts", summary_data(articles.first(count), output)}});
        articles = articles.subspan(count);
    }
    return cache.page(theme, "archives.html", context);
}

nlohmann::json taxonomy_context(SiteConfig const& config, BuildPlan const& plan,
                                PlannedTaxonomy const& taxonomy, fs::path const& output,
                                nlohmann::json const& extra) {
    auto context = collection_context(config, plan, output, extra);
    context["taxonomy"] = {{"name", taxonomy.name},
                           {"title", taxonomy.title},
                           {"url", listing_url(fs::path(taxonomy.name) / "index.html", output)}};
    return context;
}

std::string render_taxonomy(SiteConfig const& config, BuildPlan const& plan,
                            PlannedTaxonomy const& taxonomy, Theme& theme, RenderCache& cache,
                            nlohmann::json const& extra) {
    auto const output = fs::path(taxonomy.name) / "index.html";
    auto context = taxonomy_context(config, plan, taxonomy, output, extra);
    context["page"]["kind"] = "taxonomy";
    context["page"]["title"] = taxonomy.title;
    context["terms"] = term_data(taxonomy, output);
    return cache.page(theme, "taxonomy.html", context);
}

std::string render_term(SiteConfig const& config, BuildPlan const& plan,
                        PlannedTaxonomy const& taxonomy, PlannedTerm const& term, std::size_t index,
                        Theme& theme, RenderCache& cache, nlohmann::json const& extra) {
    auto const& listing = term.listings[index];
    auto context = taxonomy_context(config, plan, taxonomy, listing.output, extra);
    context["page"]["kind"] = "term";
    auto title = taxonomy.title + "：" + term.name;
    if (index != 0) {
        title += " · 第 " + std::to_string(index + 1) + " 页";
    }
    context["page"]["title"] = title;
    context["term"] = {{"name", term.name},
                       {"url", listing_url(term.listings.front().output, listing.output)},
                       {"count", term.articles.size()}};
    for (auto const article : std::span(term.articles).subspan(listing.offset, listing.count)) {
        context["posts"].push_back(summary_value(plan.articles[article], listing.output));
    }
    context["pagination"] = pagination_data(term.listings, index);
    return cache.page(theme, "term.html", context);
}

using BuildClock = std::chrono::steady_clock;

BuildStats build_site_staged(SiteConfig const& config, BuildClock::time_point start) {
    BuildStats stats;
    auto previous = start;
    auto const elapsed = [&previous] {
        auto const now = BuildClock::now();
        return now - std::exchange(previous, now);
    };
    auto const output_path = validate_output_path(config);
    auto const plan = plan_build(config);
    stats.timings.planning = elapsed();
    Theme theme(config.theme_dir, config.base_url.path);
    auto const extra = merge_extra(theme.defaults(), config.site.extra);
    OutputTransaction output(output_path);
    RenderCache cache(config.incremental ? cache_path(config, output_path, "render") : fs::path{},
                      output.stage());
    WordCounts word_counts;
    for (auto const& page : plan.pages) {
        auto const destination = output.stage() / page.output;
        fs::create_directories(destination.parent_path());
        write_file(destination, render_page(page, theme, cache, config, plan, extra, word_counts));
    }
    for (std::size_t index = 0; index < plan.listings.size(); ++index) {
        auto const destination = output.stage() / plan.listings[index].output;
        fs::create_directories(destination.parent_path());
        write_file(destination,
                   render_listing(config, plan, index, theme, cache, extra, word_counts));
    }
    if (config.archives) {
        auto const destination = output.stage() / "archives/index.html";
        fs::create_directories(destination.parent_path());
        write_file(destination, render_archives(config, plan, theme, cache, extra));
    }
    std::size_t taxonomy_pages = 0;
    for (auto const& taxonomy : plan.taxonomies) {
        if (taxonomy.terms.empty()) {
            continue;
        }
        auto const destination = output.stage() / taxonomy.name / "index.html";
        fs::create_directories(destination.parent_path());
        write_file(destination, render_taxonomy(config, plan, taxonomy, theme, cache, extra));
        ++taxonomy_pages;
        for (auto const& term : taxonomy.terms) {
            for (std::size_t index = 0; index < term.listings.size(); ++index) {
                auto const path = output.stage() / term.listings[index].output;
                fs::create_directories(path.parent_path());
                write_file(path,
                           render_term(config, plan, taxonomy, term, index, theme, cache, extra));
                ++taxonomy_pages;
            }
        }
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
        for (auto const& taxonomy : plan.taxonomies) {
            if (taxonomy.terms.empty()) {
                continue;
            }
            urls.push_back(published_url(config.base_url, taxonomy.name + "/"));
            for (auto const& term : taxonomy.terms) {
                for (auto const& listing : term.listings) {
                    urls.push_back(published_url(config.base_url,
                                                 path_to_utf8(listing.output.parent_path()) + "/"));
                }
            }
        }
        write_file(output.stage() / "sitemap.xml", render_sitemap(urls));
    } else {
        spdlog::info("Skipping RSS, sitemap and canonical URLs: set base_url to a complete HTTP(S) "
                     "address to enable them");
    }
    stats.timings.pages = elapsed();
    if (config.incremental && !config.images.compress) {
        stats.published = !output.matches_published(plan.assets);
        stats.timings.comparison = elapsed();
    }
    ImageWriter images(config.images,
                       config.images.compress ? cache_path(config, output_path, "images")
                                              : fs::path{},
                       output.stage());
    if (stats.published) {
        for (auto const& asset : plan.assets) {
            auto const destination = output.stage() / asset.output;
            fs::create_directories(destination.parent_path());
            images.write(asset.source, destination);
        }
        stats.assets_copied = plan.assets.size();
    }
    stats.timings.assets = elapsed();
    if (config.incremental && config.images.compress) {
        stats.published = !output.matches_published();
        stats.timings.comparison = elapsed();
    }
    if (stats.published) {
        output.commit();
    }
    stats.timings.publishing = elapsed();
    if (config.incremental) {
        cache.log_summary(stats.published);
    }
    images.log_summary();
    stats.pages_built =
        plan.pages.size() + plan.listings.size() + (config.archives ? 1 : 0) + taxonomy_pages;
    return stats;
}

} // namespace

BuildStats build_site(SiteConfig const& config) {
    auto const start = BuildClock::now();
    auto result = build_site_staged(config, start);
    result.timings.total = BuildClock::now() - start;
    return result;
}

} // namespace mizuki
