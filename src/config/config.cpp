#include "config/config.hpp"
#include "util/file.hpp"
#include "util/url.hpp"

#include <toml++/toml.hpp>

#include <set>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace mizuki {
namespace fs = std::filesystem;
namespace {

void check_keys(toml::table const& table, std::set<std::string_view> const& allowed) {
    for (auto const& [key, value] : table) {
        if (!allowed.contains(key.str())) {
            throw std::runtime_error("Unknown configuration key: " + std::string(key.str()));
        }
    }
}

std::string string_value(toml::table const& table, std::string_view key,
                         std::string fallback, bool allow_empty = false) {
    auto const* node = table.get(key);
    if (!node) {
        return fallback;
    }
    auto const value = node->value<std::string>();
    if (!value || (!allow_empty && value->find_first_not_of(" \t\r\n") == std::string::npos)) {
        throw std::runtime_error("Configuration '" + std::string(key) +
                                 "' must be a " + (allow_empty ? "string" : "non-empty string"));
    }
    return *value;
}

}  // namespace

BuildOptions load_config(ConfigRequest const& request) {
    BuildOptions options;
    options.config_file = fs::absolute(request.config_file).lexically_normal();
    try {
        if (fs::exists(options.config_file)) {
            auto table = toml::parse(read_file(options.config_file));
            check_keys(table, {"title", "description", "author", "language", "content", "theme",
                               "output", "navigation"});
            options.site.title = string_value(table, "title", options.site.title);
            options.site.description = string_value(table, "description", options.site.description, true);
            options.site.author = string_value(table, "author", "", true);
            options.site.language = string_value(table, "language", options.site.language);
            auto const base = options.config_file.parent_path();
            auto directory = [&](std::string_view key, fs::path const& fallback) {
                auto const value = string_value(table, key, url_path(fallback));
                if (value.find('\0') != std::string::npos) {
                    throw std::runtime_error("Configuration path '" + std::string(key) + "' contains NUL");
                }
                return (base / path_from_url(value)).lexically_normal();
            };
            options.content_dir = directory("content", options.content_dir);
            options.theme_dir = directory("theme", options.theme_dir);
            options.output_dir = directory("output", options.output_dir);
            if (auto const* navigation = table.get("navigation")) {
                auto const* items = navigation->as_array();
                if (!items) {
                    throw std::runtime_error("Configuration 'navigation' must be an array of tables");
                }
                options.site.navigation.clear();
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
                    options.site.navigation.push_back({std::move(title), std::move(url)});
                }
            }
        } else if (request.explicit_config) {
            throw std::runtime_error("Configuration file does not exist");
        }
    } catch (std::exception const& error) {
        throw std::runtime_error("Failed to load " + url_path(options.config_file) + ": " + error.what());
    }
    if (request.content_dir) {
        options.content_dir = *request.content_dir;
    }
    if (request.theme_dir) {
        options.theme_dir = *request.theme_dir;
    }
    if (request.output_dir) {
        options.output_dir = *request.output_dir;
    }
    return options;
}

}  // namespace mizuki
