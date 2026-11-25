#include "build/incremental.hpp"

#include "build/cache_version.hpp"
#include "render/theme.hpp"

#include <spdlog/spdlog.h>

#include <exception>
#include <nlohmann/json.hpp>
#include <utility>

namespace mizuki {
namespace {

using Json = nlohmann::json;

std::string markdown_key(std::string_view body, MarkdownOptions const& options) {
    return std::string("md-") +
           sha256(std::string(render_cache_version) + (options.math ? '1' : '0') +
                  (options.footnotes ? '1' : '0') + (options.admonitions ? '1' : '0') +
                  sha256(body));
}

Json encode_markdown(RenderedMarkdown const& markdown) {
    auto headings = Json::array();
    for (auto const& heading : markdown.headings) {
        headings.push_back({heading.level, heading.id, heading.text});
    }
    auto const& features = markdown.features;
    return {{"html", markdown.html},
            {"headings", std::move(headings)},
            {"features", {features.code, features.math, features.footnotes, features.admonitions}},
            {"languages", markdown.code_languages},
            {"word_count", markdown.word_count}};
}

RenderedMarkdown decode_markdown(std::string_view bytes) {
    auto const entry = Json::parse(bytes);
    auto const& features = entry.at("features");
    RenderedMarkdown markdown;
    markdown.html = entry.at("html").get<std::string>();
    markdown.features = {features.at(0).get<bool>(), features.at(1).get<bool>(),
                         features.at(2).get<bool>(), features.at(3).get<bool>()};
    markdown.code_languages = entry.at("languages").get<std::vector<std::string>>();
    markdown.word_count = entry.at("word_count").get<std::size_t>();
    for (auto const& heading : entry.at("headings")) {
        markdown.headings.push_back({heading.at(0).get<unsigned>(),
                                     heading.at(1).get<std::string>(),
                                     heading.at(2).get<std::string>()});
    }
    return markdown;
}

} // namespace

RenderCache::RenderCache(std::filesystem::path root, std::filesystem::path const& stage)
    : artifacts_(std::move(root), stage) {}

RenderedMarkdown RenderCache::markdown(std::string_view body, MarkdownOptions const& options) {
    std::string key;
    if (artifacts_.enabled()) {
        key = markdown_key(body, options);
        if (auto const cached = artifacts_.read(key)) {
            try {
                auto markdown = decode_markdown(*cached);
                ++markdown_reused_;
                return markdown;
            } catch (Json::exception const& error) {
                spdlog::debug("Invalid Markdown cache entry: {}", error.what());
            }
        }
    }

    auto markdown = render_markdown(body, options);
    ++markdown_rendered_;
    if (!key.empty()) {
        try {
            artifacts_.write(key, encode_markdown(markdown).dump());
        } catch (Json::exception const& error) {
            spdlog::debug("Cannot serialize Markdown cache entry: {}", error.what());
        }
    }
    return markdown;
}

std::string RenderCache::page(Theme& theme, std::string_view layout, Json const& context) {
    std::string key;
    if (artifacts_.enabled()) {
        try {
            key = "html-" + sha256(std::string(render_cache_version) + theme.fingerprint(layout) +
                                   sha256(context.dump()));
        } catch (Json::exception const& error) {
            spdlog::debug("Cannot serialize page context for caching: {}", error.what());
        }
        if (!key.empty()) {
            if (auto cached = artifacts_.read(key)) {
                ++pages_reused_;
                return std::move(*cached);
            }
        }
    }

    auto html = theme.render(layout, context);
    ++pages_rendered_;
    if (!key.empty()) {
        artifacts_.write(key, html);
    }
    return html;
}

void RenderCache::log_summary(bool published) const {
    spdlog::info(
        "Incremental: {} rendered, {} reused (Markdown); {} rendered, {} reused (HTML); {}",
        markdown_rendered_, markdown_reused_, pages_rendered_, pages_reused_,
        published ? "published" : "output unchanged");
}

} // namespace mizuki
