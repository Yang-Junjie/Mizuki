#include "build/context.hpp"

#include "build/links.hpp"
#include "util/path.hpp"
#include "util/url.hpp"

namespace mizuki {
namespace fs = std::filesystem;
using Json = nlohmann::json;
namespace {

Json heading_value(Heading const& heading) {
    return {{"level", heading.level},
            {"id", heading.id},
            {"text", heading.text},
            {"url", "#" + encode_url_path(heading.id)}};
}

} // namespace

std::string listing_url(fs::path const& target, fs::path const& source) {
    auto const directory = target.parent_path().empty() ? fs::path(".") : target.parent_path();
    return relative_url(directory / "", source);
}

Json page_context(SiteConfig const& config, BuildPlan const& plan, fs::path const& output,
                  Json const& extra) {
    auto const& site = config.site;
    auto navigation = Json::array();
    for (auto const& item : site.navigation) {
        navigation.push_back({{"title", item.title},
                              {"url", resolve_link(item.url, output, plan, LinkKind::Navigation)}});
    }
    auto const canonical = published_url(config.base_url, path_to_utf8(output));
    auto const url = relative_url(output, output);
    auto context = Json{
        {"site",
         {{"title", site.title},
          {"description", site.description},
          {"author", site.author},
          {"language", site.language},
          {"base_url", config.base_url.origin + config.base_url.path},
          {"base_path", config.base_url.path},
          {"extra", extra},
          {"posts", summary_data(plan.articles, output)},
          {"pages", summary_data(plan.independent_pages, output)},
          {"tags", Json::array()},
          {"categories", Json::array()}}},
        {"page",
         {{"kind", "home"},
          {"title", site.title},
          {"date", ""},
          {"description", site.description},
          {"content", ""},
          {"extra", Json::object()},
          {"tags", Json::array()},
          {"categories", Json::array()},
          {"url", url},
          {"headings", Json::array()},
          {"features",
           {{"code", false}, {"math", false}, {"footnotes", false}, {"admonitions", false}}},
          {"code_languages", Json::array()},
          {"canonical_url", canonical}}},
        {"asset_base", relative_url(fs::path("assets") / "", output)},
        {"feed_url", published_url(config.base_url, "rss.xml")},
        {"navigation", std::move(navigation)},
        {"posts", Json::array()},
        {"archives", Json::array()},
        {"toc", Json::array()},
        {"terms", Json::array()},
        {"taxonomy", {{"name", ""}, {"title", ""}, {"url", ""}}},
        {"term", {{"name", ""}, {"url", ""}, {"count", 0}}},
        {"pagination",
         {{"current_page", 1}, {"total_pages", 1}, {"previous_url", ""}, {"next_url", ""}}},
    };
    for (auto const& taxonomy : plan.taxonomies) {
        context["site"][taxonomy.name] = term_data(taxonomy, output);
    }
    return context;
}

Json term_links(std::span<std::string const> names, std::string_view taxonomy,
                fs::path const& output) {
    auto result = Json::array();
    for (auto const& name : names) {
        auto const target = path_from_utf8(taxonomy) / path_from_utf8(name) / "index.html";
        result.push_back({{"name", name}, {"url", listing_url(target, output)}});
    }
    return result;
}

Json term_data(PlannedTaxonomy const& taxonomy, fs::path const& output) {
    auto result = Json::array();
    for (auto const& term : taxonomy.terms) {
        result.push_back({{"name", term.name},
                          {"url", listing_url(term.listings.front().output, output)},
                          {"count", term.articles.size()}});
    }
    return result;
}

Json pagination_data(std::span<PlannedListing const> listings, std::size_t index) {
    auto const& output = listings[index].output;
    return {{"current_page", index + 1},
            {"total_pages", listings.size()},
            {"previous_url",
             index == 0 ? std::string{} : listing_url(listings[index - 1].output, output)},
            {"next_url", index + 1 == listings.size()
                             ? std::string{}
                             : listing_url(listings[index + 1].output, output)}};
}

Json summary_value(ContentSummary const& summary, fs::path const& output) {
    return {{"title", summary.title},
            {"date", summary.date},
            {"description", summary.description},
            {"extra", summary.extra},
            {"url", relative_url(path_from_utf8(summary.url), output)},
            {"tags", term_links(summary.tags, "tags", output)},
            {"categories", term_links(summary.categories, "categories", output)}};
}

Json summary_data(std::span<ContentSummary const> summaries, fs::path const& output) {
    auto result = Json::array();
    for (auto const& summary : summaries) {
        result.push_back(summary_value(summary, output));
    }
    return result;
}

Json heading_data(std::span<Heading const> headings) {
    auto result = Json::array();
    for (auto const& heading : headings) {
        result.push_back(heading_value(heading));
    }
    return result;
}

Json toc_data(std::span<Heading const> headings) {
    auto result = Json::array();
    bool has_parent = false;
    for (auto const& heading : headings) {
        if (heading.level == 1) {
            has_parent = false;
        }
        if (heading.level != 2 && heading.level != 3) {
            continue;
        }
        auto item = heading_value(heading);
        item["children"] = Json::array();
        if (heading.level == 3 && has_parent) {
            result.back()["children"].push_back(std::move(item));
        } else {
            result.push_back(std::move(item));
            has_parent = heading.level == 2;
        }
    }
    return result;
}

} // namespace mizuki
