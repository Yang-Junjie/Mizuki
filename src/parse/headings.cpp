#include "parse/headings.hpp"

#include "util/html.hpp"
#include "util/url.hpp"

#include <md4c-html.h>

#include <array>
#include <optional>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace mizuki {
namespace {

bool space(char ch) {
    return ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n' || ch == '\f';
}

std::string lower(std::string_view text) {
    std::string result(text);
    for (auto& ch : result) {
        if (ch >= 'A' && ch <= 'Z') {
            ch += 'a' - 'A';
        }
    }
    return result;
}

struct Attribute {
    std::size_t start;
    std::size_t end;
    std::string_view value;
};

struct Tag {
    std::size_t start;
    std::size_t end;
    std::size_t name_end;
    std::string name;
    bool closing;
};

std::size_t tag_end(std::string_view html, std::size_t start) {
    char quote = 0;
    for (auto i = start; i < html.size(); ++i) {
        auto const ch = html[i];
        if (quote) {
            if (ch == quote) {
                quote = 0;
            }
        } else if (ch == '"' || ch == '\'') {
            quote = ch;
        } else if (ch == '>') {
            return i + 1;
        }
    }
    return html.size();
}

bool raw_element(std::string_view name) {
    return name == "script" || name == "style" || name == "textarea" || name == "title" ||
           name == "xmp" || name == "iframe" || name == "noembed" || name == "noframes" ||
           name == "plaintext";
}

// Offsets always refer to the original HTML. Raw text and comments form opaque tokens.
std::vector<Tag> scan_tags(std::string_view html) {
    std::vector<Tag> tags;
    auto const folded = lower(html);
    std::size_t offset = 0;
    while ((offset = html.find('<', offset)) != std::string_view::npos) {
        auto const start = offset;
        auto end = tag_end(html, start + 1);
        if (html.substr(start).starts_with("<!--")) {
            auto const close = html.find("-->", start + 4);
            end = close == std::string_view::npos ? html.size() : close + 3;
            tags.push_back({start, end, start, {}, false});
            offset = end;
            continue;
        }
        auto name_start = start + 1;
        bool const closing = name_start < html.size() && html[name_start] == '/';
        name_start += closing;
        auto name_end = name_start;
        while (name_end < html.size() &&
               ((folded[name_end] >= 'a' && folded[name_end] <= 'z') ||
                (html[name_end] >= '0' && html[name_end] <= '9') || html[name_end] == '-' ||
                html[name_end] == '_' || html[name_end] == ':')) {
            ++name_end;
        }
        auto name = folded.substr(name_start, name_end - name_start);
        if (!closing && raw_element(name)) {
            auto close = folded.find("</" + name, end);
            while (close != std::string::npos) {
                auto const boundary = close + name.size() + 2;
                if (boundary == html.size() || space(html[boundary]) || html[boundary] == '>') {
                    break;
                }
                close = folded.find("</" + name, boundary);
            }
            end = close == std::string::npos ? html.size() : tag_end(html, close + 2);
        }
        tags.push_back({start, end, name_end, std::move(name), closing});
        offset = end;
    }
    return tags;
}

std::optional<Attribute> attribute(std::string_view html, Tag const& tag, std::string_view wanted) {
    auto i = tag.name_end;
    while (i < tag.end) {
        while (i < tag.end && space(html[i])) {
            ++i;
        }
        auto const start = i;
        while (i < tag.end && !space(html[i]) && html[i] != '=' && html[i] != '>' &&
               html[i] != '/') {
            ++i;
        }
        if (i == start) {
            break;
        }
        auto const name = lower(html.substr(start, i - start));
        while (i < tag.end && space(html[i])) {
            ++i;
        }
        if (i >= tag.end || html[i] != '=') {
            if (name == wanted) {
                return Attribute{i, i, {}};
            }
            continue;
        }
        auto const value_expression_start = i;
        ++i;
        while (i < tag.end && space(html[i])) {
            ++i;
        }
        char const quote = i < tag.end && (html[i] == '"' || html[i] == '\'') ? html[i++] : 0;
        auto const value_start = i;
        while (i < tag.end && (quote ? html[i] != quote : !space(html[i]) && html[i] != '>')) {
            ++i;
        }
        auto const value_end = i;
        if (quote && i < tag.end) {
            ++i;
        }
        if (name == wanted) {
            return Attribute{value_expression_start, i,
                             html.substr(value_start, value_end - value_start)};
        }
    }
    return std::nullopt;
}

void append_html(MD_CHAR const* text, MD_SIZE size, void* output) {
    static_cast<std::string*>(output)->append(text, size);
}

bool entity_reference(std::string_view entity) {
    auto name = entity.substr(1, entity.size() - 2);
    bool const numeric = name.starts_with('#');
    bool hex = false;
    if (numeric) {
        name.remove_prefix(1);
        hex = name.starts_with('x') || name.starts_with('X');
        if (hex) {
            name.remove_prefix(1);
        }
    }
    if (name.empty()) {
        return false;
    }
    for (char const ch : name) {
        bool const digit = ch >= '0' && ch <= '9';
        bool const letter = (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z');
        bool const hex_digit = (ch >= 'a' && ch <= 'f') || (ch >= 'A' && ch <= 'F');
        if (!(digit || (numeric ? hex && hex_digit : letter))) {
            return false;
        }
    }
    return true;
}

std::string decode_text(std::string_view text) {
    constexpr std::array entities = {
        std::pair{std::string_view("&amp;"), std::string_view("&")},
        std::pair{std::string_view("&lt;"), std::string_view("<")},
        std::pair{std::string_view("&gt;"), std::string_view(">")},
        std::pair{std::string_view("&quot;"), std::string_view("\"")},
        std::pair{std::string_view("&apos;"), std::string_view("'")},
    };
    std::string result;
    for (std::size_t i = 0; i < text.size();) {
        if (text[i] == '&') {
            bool decoded = false;
            for (auto const& [entity, value] : entities) {
                if (text.substr(i).starts_with(entity)) {
                    result += value;
                    i += entity.size();
                    decoded = true;
                    break;
                }
            }
            if (decoded) {
                continue;
            }
            auto const end = text.find(';', i + 1);
            if (end != std::string_view::npos && end - i <= 50) {
                auto const entity = text.substr(i, end - i + 1);
                if (!entity_reference(entity)) {
                    result += text[i++];
                    continue;
                }
                // md4c already decodes entities in Markdown. Reuse its complete entity table
                // for authored HTML, instead of maintaining a second table here.
                std::string rendered;
                if (md_html(entity.data(), static_cast<MD_SIZE>(entity.size()), append_html,
                            &rendered, MD_DIALECT_COMMONMARK, 0) != 0) {
                    throw std::runtime_error("Failed to decode heading entity");
                }
                if (rendered.starts_with("<p>") && rendered.ends_with("</p>\n")) {
                    auto const value = std::string_view(rendered).substr(3, rendered.size() - 8);
                    if (value != entity) {
                        result += decode_text(value);
                        i = end + 1;
                        continue;
                    }
                }
            }
        }
        result += text[i++];
    }
    return result;
}

// The same whitespace handling is used for labels and IDs, without locale-dependent casing.
std::string normalize_text(std::string_view text) {
    std::string result;
    bool pending_space = false;
    for (std::size_t i = 0; i < text.size();) {
        auto length = std::size_t{1};
        auto code = static_cast<unsigned char>(text[i]);
        unsigned point = code;
        if (code >= 0xC2 && code <= 0xF4) {
            length = code < 0xE0 ? 2 : code < 0xF0 ? 3 : 4;
            if (i + length <= text.size()) {
                point = code & (0x7F >> length);
                for (std::size_t j = 1; j < length; ++j) {
                    point = (point << 6) | (static_cast<unsigned char>(text[i + j]) & 0x3F);
                }
            } else {
                length = 1;
            }
        }
        bool const whitespace =
            (point >= 9 && point <= 13) || point == 32 || point == 0x85 || point == 0xA0 ||
            point == 0x1680 || (point >= 0x2000 && point <= 0x200A) || point == 0x2028 ||
            point == 0x2029 || point == 0x202F || point == 0x205F || point == 0x3000;
        if (whitespace) {
            pending_space = !result.empty();
        } else {
            if (pending_space) {
                result += ' ';
                pending_space = false;
            }
            result += text.substr(i, length);
        }
        i += length;
    }
    return result;
}

std::string heading_text(std::string_view html) {
    std::string text;
    std::size_t offset = 0;
    for (auto const& tag : scan_tags(html)) {
        text += decode_text(html.substr(offset, tag.start - offset));
        if (tag.name == "img") {
            if (auto const alt = attribute(html, tag, "alt")) {
                text += decode_text(alt->value);
            }
        } else if (tag.name == "br") {
            text += ' ';
        }
        offset = tag.end;
    }
    text += decode_text(html.substr(offset));
    return normalize_text(text);
}

std::string heading_id(std::string_view text) {
    std::string id;
    bool separator = false;
    for (unsigned char ch : text) {
        if (ch == ' ') {
            separator = !id.empty();
            continue;
        }
        if ((ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9') ||
            ch == '-' || ch == '_' || ch >= 0x80) {
            if (separator && id.back() != '-' && ch != '-') {
                id += '-';
            }
            separator = false;
            id += static_cast<char>(ch >= 'A' && ch <= 'Z' ? ch + ('a' - 'A') : ch);
        }
    }
    return id.empty() ? "section" : id;
}

bool heading(Tag const& tag) {
    return tag.name.size() == 2 && tag.name[0] == 'h' && tag.name[1] >= '1' && tag.name[1] <= '6';
}

} // namespace

RenderedMarkdown analyze_markdown_html(std::string_view html) {
    auto const tags = scan_tags(html);
    RenderedMarkdown result;
    unsigned pre_depth = 0;
    unsigned code_depth = 0;
    std::unordered_set<std::string> languages;
    std::unordered_set<std::string> used_ids;
    for (auto const& tag : tags) {
        if (tag.name == "pre") {
            if (!tag.closing) {
                ++pre_depth;
                result.features.code = true;
            } else if (pre_depth) {
                --pre_depth;
            }
        }
        if (!tag.closing) {
            if (auto const id = attribute(html, tag, "id")) {
                used_ids.insert(decode_text(id->value));
            }
            if (auto const classes = attribute(html, tag, "class")) {
                auto const decoded = decode_text(classes->value);
                std::string_view rest = decoded;
                while (!rest.empty()) {
                    auto const start = rest.find_first_not_of(" \t\r\n\f");
                    if (start == std::string_view::npos) {
                        break;
                    }
                    rest.remove_prefix(start);
                    auto const end = rest.find_first_of(" \t\r\n\f");
                    auto const name = rest.substr(0, end);
                    if (pre_depth && tag.name == "code" && name.starts_with("language-")) {
                        auto language = std::string(name.substr(9));
                        if (!language.empty() && languages.insert(language).second) {
                            result.code_languages.push_back(std::move(language));
                        }
                    }
                    if (!pre_depth && !code_depth) {
                        result.features.footnotes |= tag.name == "section" && name == "footnotes";
                        result.features.admonitions |=
                            tag.name == "div" && name.starts_with("admonition-");
                    }
                    if (end == std::string_view::npos) {
                        break;
                    }
                    rest.remove_prefix(end);
                }
            }
            if (!pre_depth && !code_depth && tag.name == "x-equation") {
                result.features.math = true;
            }
        }
        if (tag.name == "code") {
            if (!tag.closing) {
                ++code_depth;
            } else if (code_depth) {
                --code_depth;
            }
        }
    }
    std::unordered_map<std::string, unsigned> next_suffix;
    std::size_t offset = 0;
    code_depth = 0;
    for (std::size_t i = 0; i < tags.size(); ++i) {
        auto const& tag = tags[i];
        if (tag.name == "pre" || tag.name == "code") {
            if (!tag.closing) {
                ++code_depth;
            } else if (code_depth) {
                --code_depth;
            }
        }
        if (code_depth || tag.closing || !heading(tag)) {
            continue;
        }
        auto close = i + 1;
        while (close < tags.size() && !(tags[close].closing && tags[close].name == tag.name)) {
            ++close;
        }
        if (close == tags.size()) {
            continue;
        }
        auto const text = heading_text(html.substr(tag.end, tags[close].start - tag.end));
        auto const existing = attribute(html, tag, "id");
        auto id = existing ? decode_text(existing->value) : std::string{};
        if (id.empty()) {
            auto const base = heading_id(text);
            id = base;
            auto& suffix = next_suffix[base];
            if (suffix == 0) {
                suffix = 2;
            }
            while (!used_ids.insert(id).second) {
                id = base + "-" + std::to_string(suffix++);
            }
            result.html += html.substr(offset, tag.start - offset);
            if (existing) {
                result.html += html.substr(tag.start, existing->start - tag.start);
                result.html += "=\"" + escape_html(id) + "\"";
                result.html += html.substr(existing->end, tag.end - existing->end);
            } else {
                result.html += html.substr(tag.start, tag.end - tag.start - 1);
                result.html += " id=\"" + escape_html(id) + "\">";
            }
            offset = tag.end;
        }
        result.html += html.substr(offset, tags[close].start - offset);
        result.html += " <a class=\"heading-anchor\" href=\"#" + escape_html(encode_url_path(id)) +
                       "\" aria-label=\"" +
                       escape_html("章节链接：" + (text.empty() ? "无标题" : text)) + "\">#</a>";
        offset = tags[close].start;
        result.headings.push_back({static_cast<unsigned>(tag.name[1] - '0'), std::move(id), text});
        i = close;
    }
    result.html += html.substr(offset);
    return result;
}

} // namespace mizuki
