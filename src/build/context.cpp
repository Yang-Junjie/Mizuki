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
    return {
        {"site",
         {{"title", site.title},
          {"description", site.description},
          {"author", site.author},
          {"language", site.language},
          {"base_url", config.base_url.origin + config.base_url.path},
          {"base_path", config.base_url.path},
          {"extra", extra},
          {"posts", summary_data(plan.articles, output)},
          {"pages", summary_data(plan.independent_pages, output)}}},
        {"page",
         {{"kind", "home"},
          {"title", site.title},
          {"date", ""},
          {"description", site.description},
          {"content", ""},
          {"extra", Json::object()},
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
        {"pagination",
         {{"current_page", 1}, {"total_pages", 1}, {"previous_url", ""}, {"next_url", ""}}},
    };
}

Json summary_data(std::span<ContentSummary const> summaries, fs::path const& output) {
    auto result = Json::array();
    for (auto const& summary : summaries) {
        result.push_back({{"title", summary.title},
                          {"date", summary.date},
                          {"description", summary.description},
                          {"extra", summary.extra},
                          {"url", relative_url(path_from_utf8(summary.url), output)}});
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
