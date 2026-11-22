#pragma once

#include "config/site.hpp"
#include "render/article_list.hpp"

#include <span>
#include <string>
#include <string_view>

namespace mizuki {

struct PublicationLinks {
    std::string canonical_url;
    std::string feed_url;
    std::string head;
    std::string feed_link;
};

PublicationLinks render_publication_links(BaseUrl const& base, std::string_view output_path,
                                          std::string_view site_title);
std::string render_rss(SiteMetadata const& site, BaseUrl const& base,
                       std::span<ArticleSummary const> articles);
std::string render_sitemap(std::span<std::string const> urls);

} // namespace mizuki
