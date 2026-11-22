#include "util/url.hpp"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <stdexcept>

namespace mizuki {

namespace {

bool ipv4_address(std::string_view host) {
    unsigned parts = 0;
    while (!host.empty()) {
        auto const end = host.find('.');
        auto const part = host.substr(0, end);
        unsigned number = 0;
        auto const [last, error] = std::from_chars(part.data(), part.data() + part.size(), number);
        if (part.empty() || part.size() > 3 || error != std::errc{} ||
            last != part.data() + part.size() || number > 255) {
            return false;
        }
        ++parts;
        if (end == std::string_view::npos) {
            return parts == 4;
        }
        host.remove_prefix(end + 1);
    }
    return false;
}

bool ipv6_address(std::string_view host) {
    auto const compression = host.find("::");
    bool const compressed = compression != std::string_view::npos;
    if (host.empty() || host.find(":::") != std::string_view::npos ||
        (compressed && host.find("::", compression + 2) != std::string_view::npos) ||
        (host.starts_with(':') && !host.starts_with("::")) ||
        (host.ends_with(':') && !host.ends_with("::"))) {
        return false;
    }
    unsigned groups = 0;
    while (!host.empty()) {
        auto const end = host.find(':');
        auto const part = host.substr(0, end);
        if (part.find('.') != std::string_view::npos) {
            return end == std::string_view::npos && ipv4_address(part) &&
                   (compressed ? groups + 2 < 8 : groups + 2 == 8);
        }
        if (!part.empty()) {
            if (part.size() > 4 || !std::ranges::all_of(part, [](unsigned char ch) {
                    return std::isxdigit(ch) != 0;
                })) {
                return false;
            }
            ++groups;
        }
        if (end == std::string_view::npos) {
            break;
        }
        host.remove_prefix(end + 1);
    }
    return compressed ? groups < 8 : groups == 8;
}

void validate_authority(std::string_view authority) {
    auto host = authority;
    std::string_view port;
    if (authority.starts_with('[')) {
        auto const end = authority.find(']');
        if (end == std::string_view::npos) {
            throw std::runtime_error("Invalid base_url host");
        }
        host = authority.substr(1, end - 1);
        if (!ipv6_address(host)) {
            throw std::runtime_error("Invalid base_url IPv6 host");
        }
        if (end + 1 < authority.size()) {
            if (authority[end + 1] != ':') {
                throw std::runtime_error("Invalid base_url host");
            }
            port = authority.substr(end + 2);
        }
    } else {
        auto const colon = authority.find(':');
        host = authority.substr(0, colon);
        if (colon != std::string_view::npos) {
            port = authority.substr(colon + 1);
        }
        if (!std::ranges::all_of(host, [](unsigned char ch) {
                return std::isalnum(ch) || ch == '-' || ch == '.';
            })) {
            throw std::runtime_error("Invalid base_url host (credentials are not supported)");
        }
    }
    if (host.empty()) {
        throw std::runtime_error("base_url requires a host");
    }
    if (!port.empty()) {
        unsigned number = 0;
        auto const [end, error] = std::from_chars(port.data(), port.data() + port.size(), number);
        if (error != std::errc{} || end != port.data() + port.size() || number == 0 ||
            number > 65535) {
            throw std::runtime_error("Invalid base_url port");
        }
    } else if (authority.ends_with(':')) {
        throw std::runtime_error("Invalid base_url port");
    }
}

} // namespace

BaseUrl parse_base_url(std::string_view url) {
    if (url.empty() || url.find_first_of("?#\\") != std::string_view::npos ||
        std::ranges::any_of(url, [](unsigned char ch) { return ch < 0x20 || ch == 0x7F; })) {
        throw std::runtime_error("base_url must be an HTTP(S) URL or an absolute site path, "
                                 "without a query or fragment");
    }
    BaseUrl base;
    auto path = url;
    if (!url.starts_with('/')) {
        auto const separator = url.find("://");
        auto scheme = std::string(url.substr(0, separator));
        std::ranges::transform(scheme, scheme.begin(), [](unsigned char ch) {
            return static_cast<char>(std::tolower(ch));
        });
        if (separator == std::string_view::npos || (scheme != "http" && scheme != "https")) {
            throw std::runtime_error("base_url must use http://, https://, or start with '/'");
        }
        auto const host_start = separator + 3;
        auto const path_start = url.find('/', host_start);
        auto const authority = url.substr(host_start, path_start - host_start);
        validate_authority(authority);
        base.origin = scheme + "://" + std::string(authority);
        path =
            path_start == std::string_view::npos ? std::string_view("/") : url.substr(path_start);
    }
    if (path.starts_with("//")) {
        throw std::runtime_error("base_url path must start with a single '/'");
    }
    for (std::size_t index = 0; index < path.size(); ++index) {
        if (path[index] == '%') {
            if (index + 2 >= path.size() ||
                !std::isxdigit(static_cast<unsigned char>(path[index + 1])) ||
                !std::isxdigit(static_cast<unsigned char>(path[index + 2]))) {
                throw std::runtime_error("Invalid percent encoding in base_url");
            }
            index += 2;
        }
    }
    std::string decoded;
    try {
        decoded = decode_url_path(path);
    } catch (std::exception const& error) {
        throw std::runtime_error("Invalid base_url path: " + std::string(error.what()));
    }
    if (decoded.find("//") != std::string::npos || decoded.find(':') != std::string::npos ||
        std::ranges::any_of(decoded, [](unsigned char ch) { return ch < 0x20 || ch == 0x7F; })) {
        throw std::runtime_error("Invalid base_url path");
    }
    std::size_t start = 1;
    while (start < decoded.size()) {
        auto const end = decoded.find('/', start);
        auto const component = std::string_view(decoded).substr(start, end - start);
        if (component == "." || component == "..") {
            throw std::runtime_error("base_url path cannot contain '.' or '..'");
        }
        start = end == std::string::npos ? decoded.size() : end + 1;
    }
    if (!decoded.ends_with('/')) {
        decoded += '/';
    }
    base.path = encode_url_path(decoded);
    return base;
}

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

std::string published_url(BaseUrl const& base, std::string_view output_path) {
    if (base.origin.empty()) {
        return {};
    }
    return base.origin + base.path + encode_url_path(output_path);
}

} // namespace mizuki
