#include "parse/front_matter.hpp"

#include "util/date.hpp"

#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <utility>

namespace mizuki {

namespace {

std::string_view trim(std::string_view text) {
    auto const is_space = [](char ch) { return std::isspace(static_cast<unsigned char>(ch)) != 0; };
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
    return std::ranges::equal(lhs, rhs, [](char left, char right) {
        return std::tolower(static_cast<unsigned char>(left)) ==
               std::tolower(static_cast<unsigned char>(right));
    });
}

} // namespace

Document parse_document(std::string_view source, DocumentKind kind) {
    FrontMatter metadata;

    constexpr std::string_view bom = "\xEF\xBB\xBF";
    if (source.starts_with(bom)) {
        source.remove_prefix(bom.size());
    }

    auto remaining = source;
    if (trim(next_line(remaining)) != "---") {
        throw std::runtime_error("Missing front matter");
    }

    bool has_title = false;
    bool has_date = false;
    bool has_draft = false;
    bool has_description = false;
    bool closed = false;
    std::string line_error;

    while (!remaining.empty()) {
        auto const line = trim(next_line(remaining));
        if (line == "---") {
            closed = true;
            break;
        }
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

    if (!closed) {
        throw std::runtime_error("Invalid front matter: missing closing '---'");
    }
    if (!line_error.empty()) {
        throw std::runtime_error(std::move(line_error));
    }
    if (!has_title || metadata.title.empty()) {
        throw std::runtime_error("Invalid front matter: missing required key 'title'");
    }
    if ((kind == DocumentKind::Article && !has_date) || (has_date && metadata.date.empty())) {
        throw std::runtime_error("Invalid front matter: missing required key 'date'");
    }

    if (has_date) {
        parse_date(metadata.date);
    }

    return {std::move(metadata), std::string(remaining)};
}

} // namespace mizuki
