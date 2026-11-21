#include "build/links.hpp"

#include "build/plan.hpp"
#include "util/path.hpp"
#include "util/url.hpp"

#include <algorithm>
#include <cctype>
#include <stdexcept>

namespace mizuki {
namespace fs = std::filesystem;
namespace {

std::string lower(std::string_view text) {
    std::string result(text);
    std::ranges::transform(result, result.begin(),
                           [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return result;
}

} // namespace

std::string resolve_link(std::string_view url, fs::path const& source, BuildPlan const& plan,
                         LinkKind kind) {
    bool const navigation = kind == LinkKind::Navigation;
    if (has_url_scheme(url) || url.starts_with("//")) {
        auto const scheme = lower(url.substr(0, url.find(':')));
        if (navigation && !url.starts_with("//") && scheme != "http" && scheme != "https") {
            throw std::runtime_error("Navigation URL must be local or use http/https: " +
                                     std::string(url));
        }
        return std::string(url);
    }
    auto const separator = url.find_first_of("?#");
    auto const raw_path = url.substr(0, separator);
    auto const suffix =
        separator == std::string_view::npos ? std::string_view{} : url.substr(separator);
    if (raw_path.empty()) {
        return std::string(url);
    }
    auto decoded = decode_url_path(raw_path);
    bool const rooted = decoded.starts_with('/');
    auto path = path_from_utf8(decoded);
    if (rooted) {
        path = path.relative_path();
    } else if (!navigation) {
        path = source.parent_path() / path;
    }
    path = path.lexically_normal();
    if (path.is_absolute() || path.has_root_name() ||
        (!path.empty() && *path.begin() == fs::path(".."))) {
        throw std::runtime_error("Local link escapes content directory in " + path_to_utf8(source) +
                                 ": " + std::string(url));
    }
    bool const markdown = is_markdown(path);
    if (markdown) {
        auto const found = plan.page_urls.find(path);
        if (found == plan.page_urls.end()) {
            throw std::runtime_error("Markdown link target is missing or unpublished in " +
                                     path_to_utf8(source) + ": " + std::string(url));
        }
        path = found->second;
    } else if (navigation) {
        if (decoded.ends_with('/') || path == fs::path(".")) {
            path /= "index.html";
        }
        path = path.lexically_normal();
        if (!plan.outputs.contains(path)) {
            throw std::runtime_error("Navigation target does not exist: " + std::string(url));
        }
    } else {
        return std::string(url);
    }
    if (navigation || !rooted) {
        path = path.lexically_relative(source.parent_path());
    }
    auto result = encode_url_path(path_to_utf8(path));
    if (rooted && !navigation) {
        result.insert(0, "/");
    }
    // A colon in the first component must never turn a relative link into a scheme.
    if (!rooted && result.find(':') < result.find('/')) {
        result.insert(0, "./");
    }
    result += suffix;
    return result;
}

} // namespace mizuki
