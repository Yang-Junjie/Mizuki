#pragma once

#include "build/artifact_cache.hpp"
#include "parse/markdown.hpp"

#include <cstddef>
#include <nlohmann/json_fwd.hpp>

namespace mizuki {

class Theme;

class RenderCache {
public:
    RenderCache(std::filesystem::path root, std::filesystem::path const& stage);

    RenderedMarkdown markdown(std::string_view body, MarkdownOptions const& options);
    std::string page(Theme& theme, std::string_view layout, nlohmann::json const& context);
    void log_summary(bool published) const;

private:
    ArtifactCache artifacts_;
    std::size_t markdown_rendered_{0};
    std::size_t markdown_reused_{0};
    std::size_t pages_rendered_{0};
    std::size_t pages_reused_{0};
};

} // namespace mizuki
