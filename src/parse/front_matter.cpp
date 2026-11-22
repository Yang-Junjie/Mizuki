#include "parse/front_matter.hpp"

#include "config/values.hpp"
#include "util/date.hpp"

#include <toml++/toml.hpp>

#include <algorithm>
#include <array>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace mizuki {

namespace {

std::string_view trim(std::string_view text) {
    auto const is_space = [](char ch) { return ch == ' ' || (ch >= '\t' && ch <= '\r'); };
    while (!text.empty() && is_space(text.front())) {
        text.remove_prefix(1);
    }
    while (!text.empty() && is_space(text.back())) {
        text.remove_suffix(1);
    }
    return text;
}

std::string_view next_line(std::string_view& remaining) {
    auto const newline = remaining.find('\n');
    std::string_view const line =
        newline == std::string_view::npos ? remaining : remaining.substr(0, newline);
    remaining =
        newline == std::string_view::npos ? std::string_view{} : remaining.substr(newline + 1);
    return line;
}

std::string_view unquote(std::string_view value) {
    if (value.size() >= 2 && ((value.front() == '"' && value.back() == '"') ||
                              (value.front() == '\'' && value.back() == '\''))) {
        value.remove_prefix(1);
        value.remove_suffix(1);
    }
    return value;
}

bool equals_ignore_case(std::string_view lhs, std::string_view rhs) {
    auto const lowercase = [](char ch) {
        return ch >= 'A' && ch <= 'Z' ? static_cast<char>(ch - 'A' + 'a') : ch;
    };
    return std::ranges::equal(lhs, rhs, {}, lowercase, lowercase);
}

std::vector<std::string> term_names(toml::node const& node, std::string_view key) {
    auto const* array = node.as_array();
    if (!array) {
        throw std::runtime_error("Invalid front matter: '" + std::string(key) +
                                 "' must be an array of strings");
    }
    std::vector<std::string> names;
    for (auto const& value : *array) {
        auto const text = value.value<std::string>();
        if (!value.is_string() || !text) {
            throw std::runtime_error("Invalid front matter: '" + std::string(key) +
                                     "' must be an array of strings");
        }
        auto const name = std::string(trim(*text));
        // Each term names one portable output directory. Do not silently change its identity.
        auto const invalid =
            name.empty() || name == "." || name == ".." || name.ends_with('.') ||
            std::ranges::any_of(name, [](unsigned char ch) {
                return ch < 32 || ch == 127 ||
                       std::string_view("/\\<>:\"|?*").find(ch) != std::string_view::npos;
            });
        auto const stem = std::string_view(name).substr(0, name.find('.'));
        bool const device = equals_ignore_case(stem, "con") || equals_ignore_case(stem, "prn") ||
                            equals_ignore_case(stem, "aux") || equals_ignore_case(stem, "nul") ||
                            (stem.size() == 4 && stem.back() >= '1' && stem.back() <= '9' &&
                             (equals_ignore_case(stem.substr(0, 3), "com") ||
                              equals_ignore_case(stem.substr(0, 3), "lpt")));
        if (invalid || device) {
            throw std::runtime_error("Invalid front matter: '" + std::string(key) +
                                     "' contains an invalid term name: " + name);
        }
        if (std::ranges::find(names, name) == names.end()) {
            names.push_back(name);
        }
    }
    return names;
}

FrontMatter parse_literal_metadata(std::string_view remaining) {
    FrontMatter metadata;
    bool has_title = false;
    bool has_date = false;
    bool has_draft = false;
    bool has_description = false;
    bool has_layout = false;
    bool has_tags = false;
    bool has_categories = false;
    std::string line_error;

    while (!remaining.empty()) {
        auto const line = trim(next_line(remaining));
        if (line.empty()) {
            continue;
        }

        auto const colon = line.find(':');
        if (colon == std::string_view::npos) {
            if (line_error.empty()) {
                line_error = "Invalid front matter line: " + std::string(line);
            }
            continue;
        }

        auto const key = trim(line.substr(0, colon));
        auto const value = unquote(trim(line.substr(colon + 1)));
        if (key.empty()) {
            throw std::runtime_error("Invalid front matter line: " + std::string(line));
        }

        if (key == "title") {
            if (has_title) {
                throw std::runtime_error("Duplicate front matter key: title");
            }
            has_title = true;
            metadata.title = value;
        } else if (key == "date") {
            if (has_date) {
                throw std::runtime_error("Duplicate front matter key: date");
            }
            has_date = true;
            metadata.date = value;
        } else if (key == "description") {
            if (has_description) {
                throw std::runtime_error("Duplicate front matter key: description");
            }
            has_description = true;
            metadata.description = value;
        } else if (key == "layout") {
            if (has_layout) {
                throw std::runtime_error("Duplicate front matter key: layout");
            }
            has_layout = true;
            if (value.empty()) {
                throw std::runtime_error("Invalid front matter: 'layout' must be non-empty");
            }
            metadata.layout = value;
        } else if (key == "tags" || key == "categories") {
            auto& seen = key == "tags" ? has_tags : has_categories;
            if (seen) {
                throw std::runtime_error("Duplicate front matter key: " + std::string(key));
            }
            seen = true;
            try {
                auto const table = toml::parse("terms = " + std::string(value));
                auto const* terms = table.get("terms");
                if (!terms) {
                    throw std::runtime_error("Invalid front matter: '" + std::string(key) +
                                             "' must be an array of strings");
                }
                (key == "tags" ? metadata.tags : metadata.categories) = term_names(*terms, key);
            } catch (toml::parse_error const& error) {
                throw std::runtime_error("Invalid front matter: '" + std::string(key) +
                                         "' must be an inline TOML array of strings: " +
                                         std::string(error.description()));
            }
        } else if (key == "draft") {
            if (has_draft) {
                throw std::runtime_error("Duplicate front matter key: draft");
            }
            has_draft = true;
            if (equals_ignore_case(value, "true")) {
                metadata.draft = true;
            } else if (!equals_ignore_case(value, "false")) {
                throw std::runtime_error("Invalid front matter: 'draft' must be true or false");
            }
        }
    }

    if (!line_error.empty()) {
        throw std::runtime_error(std::move(line_error));
    }
    if (has_date && metadata.date.empty()) {
        throw std::runtime_error("Invalid front matter: missing required key 'date'");
    }
    return metadata;
}

std::string string_value(toml::node const& node, std::string_view key) {
    if (!node.is_string()) {
        throw std::runtime_error("Invalid front matter: '" + std::string(key) +
                                 "' must be a string");
    }
    return *node.value<std::string>();
}

FrontMatter parse_toml_metadata(std::string_view source) {
    // The opening delimiter occupies line 1 in the source document.
    toml::table table;
    try {
        table = toml::parse("\n" + std::string(source));
    } catch (toml::parse_error const& error) {
        std::ostringstream message;
        message << error;
        throw std::runtime_error(message.str());
    }
    FrontMatter metadata;
    constexpr std::array<std::string_view, 8> keys{"title", "date",  "description", "layout",
                                                   "draft", "extra", "tags",        "categories"};
    for (auto const& [key, value] : table) {
        auto const name = key.str();
        if (std::ranges::find(keys, name) == keys.end()) {
            throw std::runtime_error("Unknown front matter key: " + std::string(name) +
                                     "; put custom data in [extra]");
        }
        if (name == "title") {
            metadata.title = string_value(value, name);
        } else if (name == "description") {
            metadata.description = string_value(value, name);
        } else if (name == "layout") {
            metadata.layout = string_value(value, name);
            if (trim(metadata.layout).empty()) {
                throw std::runtime_error("Invalid front matter: 'layout' must be non-empty");
            }
        } else if (name == "date") {
            if (auto const* date = value.as_date()) {
                std::ostringstream formatted;
                formatted << date->get();
                metadata.date = formatted.str();
            } else {
                metadata.date = string_value(value, name);
            }
            if (metadata.date.empty()) {
                throw std::runtime_error("Invalid front matter: 'date' must be non-empty");
            }
        } else if (name == "draft") {
            if (!value.is_boolean()) {
                throw std::runtime_error("Invalid front matter: 'draft' must be a boolean");
            }
            metadata.draft = *value.value<bool>();
        } else if (name == "tags" || name == "categories") {
            (name == "tags" ? metadata.tags : metadata.categories) = term_names(value, name);
        } else if (name == "extra") {
            if (!value.is_table()) {
                throw std::runtime_error("Invalid front matter: 'extra' must be a table");
            }
            metadata.extra = template_value(value);
        }
    }
    return metadata;
}

} // namespace

Document parse_document(std::string_view source, DocumentKind kind) {
    constexpr std::string_view bom = "\xEF\xBB\xBF";
    if (source.starts_with(bom)) {
        source.remove_prefix(bom.size());
    }
    auto remaining = source;
    auto const delimiter = trim(next_line(remaining));
    if (delimiter != "---" && delimiter != "+++") {
        throw std::runtime_error("Missing front matter: expected '+++' or '---'");
    }
    auto const header = remaining;
    while (!remaining.empty()) {
        auto const before = remaining;
        if (trim(next_line(remaining)) != delimiter) {
            continue;
        }
        auto const fields = header.substr(0, header.size() - before.size());
        auto metadata =
            delimiter == "+++" ? parse_toml_metadata(fields) : parse_literal_metadata(fields);
        if (trim(metadata.title).empty()) {
            throw std::runtime_error("Invalid front matter: missing required key 'title'");
        }
        if (metadata.date.empty() && kind == DocumentKind::Article) {
            throw std::runtime_error("Invalid front matter: missing required key 'date'");
        }
        if (!metadata.date.empty()) {
            parse_date(metadata.date);
        }
        if (kind == DocumentKind::Page &&
            (!metadata.tags.empty() || !metadata.categories.empty())) {
            throw std::runtime_error("Invalid front matter: tags and categories are only supported "
                                     "on articles, not independent pages");
        }
        return {std::move(metadata), std::string(remaining)};
    }
    throw std::runtime_error("Invalid front matter: missing closing '" + std::string(delimiter) +
                             "'");
}

} // namespace mizuki
