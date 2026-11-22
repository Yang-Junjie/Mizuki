#include "render/html_links.hpp"

#include "util/html.hpp"

#include <cctype>
#include <charconv>
#include <optional>

namespace mizuki {
namespace {

std::string lower(std::string_view text) {
    std::string result(text);
    for (auto& ch : result) {
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    return result;
}

bool space(char ch) {
    return std::isspace(static_cast<unsigned char>(ch)) != 0;
}

void append_utf8(std::string& output, unsigned code) {
    if (code <= 0x7F) {
        output += static_cast<char>(code);
    } else if (code <= 0x7FF) {
        output += static_cast<char>(0xC0 | (code >> 6));
        output += static_cast<char>(0x80 | (code & 63));
    } else if (code <= 0xFFFF) {
        output += static_cast<char>(0xE0 | (code >> 12));
        output += static_cast<char>(0x80 | ((code >> 6) & 63));
        output += static_cast<char>(0x80 | (code & 63));
    } else {
        output += static_cast<char>(0xF0 | (code >> 18));
        output += static_cast<char>(0x80 | ((code >> 12) & 63));
        output += static_cast<char>(0x80 | ((code >> 6) & 63));
        output += static_cast<char>(0x80 | (code & 63));
    }
}

// md4c emits escaped attributes. Decode before resolving, then escape once on output.
std::string decode_attribute(std::string_view text) {
    std::string output;
    for (std::size_t i = 0; i < text.size(); ++i) {
        if (text[i] == '&') {
            auto const end = text.find(';', i + 1);
            if (end != std::string_view::npos && end - i <= 12) {
                auto entity = text.substr(i + 1, end - i - 1);
                unsigned code = 0;
                if (entity == "amp") {
                    code = '&';
                } else if (entity == "quot") {
                    code = '"';
                } else if (entity == "apos") {
                    code = '\'';
                } else if (entity == "lt") {
                    code = '<';
                } else if (entity == "gt") {
                    code = '>';
                } else if (entity.starts_with('#')) {
                    entity.remove_prefix(1);
                    int base = 10;
                    if (entity.starts_with('x') || entity.starts_with('X')) {
                        entity.remove_prefix(1);
                        base = 16;
                    }
                    auto const [last, error] =
                        std::from_chars(entity.data(), entity.data() + entity.size(), code, base);
                    if (error != std::errc{} || last != entity.data() + entity.size()) {
                        code = 0;
                    }
                }
                if (code != 0 && code <= 0x10FFFF && !(code >= 0xD800 && code <= 0xDFFF)) {
                    append_utf8(output, code);
                    i = end;
                    continue;
                }
            }
        }
        output += text[i];
    }
    return output;
}

std::size_t tag_end(std::string_view html, std::size_t start) {
    char quote = 0;
    for (auto i = start; i < html.size(); ++i) {
        char const ch = html[i];
        if (quote) {
            if (ch == quote) {
                quote = 0;
            }
        } else if (ch == '"' || ch == '\'') {
            quote = ch;
        } else if (ch == '>') {
            return i;
        }
    }
    return std::string_view::npos;
}

std::optional<HtmlLinkKind> attribute_kind(std::string_view element, std::string_view attribute) {
    if ((element == "a" || element == "area") && attribute == "href") {
        return HtmlLinkKind::Anchor;
    }
    if ((attribute == "href" && element == "link") ||
        (attribute == "poster" && element == "video") ||
        (attribute == "data" && element == "object") ||
        (attribute == "src" && (element == "img" || element == "source" || element == "script" ||
                                element == "audio" || element == "video" || element == "iframe" ||
                                element == "input" || element == "track" || element == "embed"))) {
        return HtmlLinkKind::Resource;
    }
    return std::nullopt;
}

std::string rewrite_tag(std::string_view tag, std::size_t name_end, std::string_view element,
                        LinkRewriter const& rewrite) {
    std::string result;
    std::size_t copied = 0;
    auto i = name_end;
    while (i < tag.size()) {
        while (i < tag.size() && space(tag[i])) {
            ++i;
        }
        auto const name_start = i;
        while (i < tag.size() && !space(tag[i]) && tag[i] != '=' && tag[i] != '>' &&
               tag[i] != '/') {
            ++i;
        }
        if (i == name_start) {
            break;
        }
        auto const name = lower(tag.substr(name_start, i - name_start));
        while (i < tag.size() && space(tag[i])) {
            ++i;
        }
        if (i == tag.size() || tag[i] != '=') {
            continue;
        }
        ++i;
        while (i < tag.size() && space(tag[i])) {
            ++i;
        }
        if (i == tag.size()) {
            break;
        }
        char const quote = tag[i] == '"' || tag[i] == '\'' ? tag[i++] : 0;
        auto const start = i;
        while (i < tag.size() && (quote ? tag[i] != quote : !space(tag[i]) && tag[i] != '>')) {
            ++i;
        }
        auto const end = i;
        if (quote && i < tag.size()) {
            ++i;
        }
        auto const kind = attribute_kind(element, name);
        if (!kind) {
            continue;
        }
        auto const old = decode_attribute(tag.substr(start, end - start));
        auto const rewritten = rewrite(old, *kind);
        if (rewritten == old) {
            continue;
        }
        auto const attribute_start = quote ? start - 1 : start;
        auto const attribute_end = quote && end < tag.size() ? end + 1 : end;
        result += tag.substr(copied, attribute_start - copied);
        result += "\"" + escape_html(rewritten) + "\"";
        copied = attribute_end;
    }
    result += tag.substr(copied);
    return result;
}

} // namespace

std::string rewrite_document_links(std::string_view html, LinkRewriter const& rewrite) {
    std::string result;
    std::string raw_element;
    auto const folded = lower(html);
    std::size_t offset = 0;
    while (offset < html.size()) {
        if (!raw_element.empty()) {
            auto close = folded.find("</" + raw_element, offset);
            while (close != std::string::npos) {
                auto const boundary = close + raw_element.size() + 2;
                if (boundary == html.size() || space(html[boundary]) || html[boundary] == '>') {
                    break;
                }
                close = folded.find("</" + raw_element, boundary);
            }
            if (close == std::string::npos) {
                result += html.substr(offset);
                break;
            }
            result += html.substr(offset, close - offset);
            offset = close;
            raw_element.clear();
        }
        auto const start = html.find('<', offset);
        if (start == std::string_view::npos) {
            result += html.substr(offset);
            break;
        }
        result += html.substr(offset, start - offset);
        if (html.substr(start).starts_with("<!--")) {
            auto end = html.find("-->", start + 4);
            end = end == std::string_view::npos ? html.size() : end + 3;
            result += html.substr(start, end - start);
            offset = end;
            continue;
        }
        auto const end = tag_end(html, start + 1);
        if (end == std::string_view::npos) {
            result += html.substr(start);
            break;
        }
        auto name_end = start + 1;
        while (name_end < end &&
               (std::isalnum(static_cast<unsigned char>(html[name_end])) || html[name_end] == '-' ||
                html[name_end] == '_' || html[name_end] == ':')) {
            ++name_end;
        }
        auto const name = lower(html.substr(start + 1, name_end - start - 1));
        auto const tag = html.substr(start, end - start + 1);
        result += rewrite_tag(tag, name_end - start, name, rewrite);
        if (name == "script" || name == "style" || name == "textarea" || name == "title") {
            raw_element = name;
        }
        offset = end + 1;
    }
    return result;
}

} // namespace mizuki
