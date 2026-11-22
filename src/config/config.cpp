#include "config/config.hpp"

#include "util/file.hpp"
#include "util/path.hpp"

#include <toml++/toml.hpp>

#include <algorithm>
#include <exception>
#include <initializer_list>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <vector>

namespace mizuki {
namespace fs = std::filesystem;
namespace {

enum class EmptyString { Reject, Allow };

void check_keys(toml::table const& table, std::initializer_list<std::string_view> allowed) {
    for (auto const& [key, value] : table) {
        if (std::ranges::find(allowed, key.str()) == allowed.end()) {
            throw std::runtime_error("Unknown configuration key: " + std::string(key.str()));
        }
    }
}

std::string string_value(toml::table const& table, std::string_view key, std::string fallback,
                         EmptyString empty = EmptyString::Reject) {
    auto const* node = table.get(key);
    if (!node) {
        return fallback;
    }
    auto const value = node->value<std::string>();
    if (!value || (empty == EmptyString::Reject &&
                   value->find_first_not_of(" \t\r\n") == std::string::npos)) {
        throw std::runtime_error("Configuration '" + std::string(key) + "' must be a " +
                                 (empty == EmptyString::Allow ? "string" : "non-empty string"));
    }
    return *value;
}

fs::path directory_value(toml::table const& table, std::string_view key, fs::path const& fallback,
                         fs::path const& base) {
    auto const value = string_value(table, key, path_to_utf8(fallback));
    if (value.find('\0') != std::string::npos) {
        throw std::runtime_error("Configuration path '" + std::string(key) + "' contains NUL");
    }
    return (base / path_from_utf8(value)).lexically_normal();
}

std::vector<NavigationItem> parse_navigation(toml::node const& node) {
    auto const* items = node.as_array();
    if (!items) {
        throw std::runtime_error("Configuration 'navigation' must be an array of tables");
    }
    std::vector<NavigationItem> navigation;
    for (auto const& item : *items) {
        auto const* entry = item.as_table();
        if (!entry) {
            throw std::runtime_error("Each navigation item must be a table");
        }
        check_keys(*entry, {"title", "url"});
        auto title = string_value(*entry, "title", "");
        auto url = string_value(*entry, "url", "");
        if (title.empty() || url.empty()) {
            throw std::runtime_error("Navigation items require 'title' and 'url'");
        }
        navigation.push_back({std::move(title), std::move(url)});
    }
    return navigation;
}

void read_config_file(SiteConfig& config) {
    auto const table = toml::parse(read_file(config.config_file));
    check_keys(table, {"title", "description", "author", "language", "content", "theme", "output",
                       "navigation", "base_url"});
    config.base_url = parse_base_url(string_value(table, "base_url", "/"));
    auto& site = config.site;
    site.title = string_value(table, "title", site.title);
    site.description = string_value(table, "description", site.description, EmptyString::Allow);
    site.author = string_value(table, "author", site.author, EmptyString::Allow);
    site.language = string_value(table, "language", site.language);
    auto const base = config.config_file.parent_path();
    config.content_dir = directory_value(table, "content", config.content_dir, base);
    config.theme_dir = directory_value(table, "theme", config.theme_dir, base);
    config.output_dir = directory_value(table, "output", config.output_dir, base);
    if (auto const* navigation = table.get("navigation")) {
        site.navigation = parse_navigation(*navigation);
    }
}

} // namespace

SiteConfig load_config(ConfigRequest const& request) {
    SiteConfig config;
    config.config_file = fs::absolute(request.config_file).lexically_normal();
    try {
        if (fs::exists(config.config_file)) {
            read_config_file(config);
        } else if (request.explicit_config) {
            throw std::runtime_error("Configuration file does not exist");
        }
    } catch (std::exception const& error) {
        throw std::runtime_error("Failed to load " + path_to_utf8(config.config_file) + ": " +
                                 error.what());
    }
    if (request.content_dir) {
        config.content_dir = *request.content_dir;
    }
    if (request.theme_dir) {
        config.theme_dir = *request.theme_dir;
    }
    if (request.output_dir) {
        config.output_dir = *request.output_dir;
    }
    if (request.base_url) {
        config.base_url = parse_base_url(*request.base_url);
    }
    return config;
}

} // namespace mizuki
