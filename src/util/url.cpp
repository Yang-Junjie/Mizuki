#include "util/url.hpp"

#include <cctype>
#include <stdexcept>

namespace mizuki {

std::string encode_url_path(std::string_view path) {
    constexpr char hex[] = "0123456789ABCDEF";
    std::string result;
    for (unsigned char ch : path) {
        if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') ||
            ch == '-' || ch == '.' || ch == '_' || ch == '~' || ch == '/') {
            result += static_cast<char>(ch);
        } else {
            result += '%';
            result += hex[ch >> 4];
            result += hex[ch & 15];
        }
    }
    return result;
}

std::string decode_url_path(std::string_view path) {
    auto hex = [](unsigned char ch) -> int {
        if (ch >= '0' && ch <= '9') {
            return ch - '0';
        }
        if (ch >= 'a' && ch <= 'f') {
            return ch - 'a' + 10;
        }
        if (ch >= 'A' && ch <= 'F') {
            return ch - 'A' + 10;
        }
        return -1;
    };
    std::string result;
    for (std::size_t index = 0; index < path.size(); ++index) {
        char ch = path[index];
        if (ch == '%' && index + 2 < path.size()) {
            auto const high = hex(path[index + 1]);
            auto const low = hex(path[index + 2]);
            if (high >= 0 && low >= 0) {
                ch = static_cast<char>((high << 4) | low);
                index += 2;
            }
        }
        if (ch == '\0' || ch == '\\') {
            throw std::runtime_error("Invalid character in local URL path");
        }
        result += ch;
    }
    return result;
}

bool has_url_scheme(std::string_view url) {
    if (url.empty() || !std::isalpha(static_cast<unsigned char>(url.front()))) {
        return false;
    }
    for (char ch : url) {
        if (ch == ':') {
            return true;
        }
        if (!std::isalnum(static_cast<unsigned char>(ch)) && ch != '+' && ch != '-' && ch != '.') {
            return false;
        }
    }
    return false;
}

} // namespace mizuki
