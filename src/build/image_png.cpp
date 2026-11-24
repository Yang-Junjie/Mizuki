#include "build/image_codecs.hpp"

#include <array>
#include <cstdint>
#include <string>
// Match the symbol prefix used by libjpeg-turbo's bundled zlib.
#define Z_PREFIX
#include <zlib.h>
#undef Z_PREFIX

namespace mizuki {
namespace {

constexpr std::string_view png_signature{"\x89PNG\r\n\x1a\n", 8};
constexpr std::size_t chunk_overhead = 12;
constexpr std::size_t scanline_limit = 128 * 1024 * 1024;

std::uint32_t read_u32(std::string_view data) {
    std::uint32_t value = 0;
    for (unsigned char byte : data.substr(0, 4)) {
        value = (value << 8) | byte;
    }
    return value;
}

void append_u32(std::string& result, std::uint32_t value) {
    for (int shift : {24, 16, 8, 0}) {
        result.push_back(static_cast<char>((value >> shift) & 0xff));
    }
}

std::uint32_t chunk_crc(std::string_view bytes) {
    return static_cast<std::uint32_t>(
        crc32_z(0, reinterpret_cast<Bytef const*>(bytes.data()), bytes.size()));
}

std::optional<std::string> inflate_scanlines(std::string& compressed) {
    z_stream stream{};
    if (inflateInit(&stream) != Z_OK) {
        return std::nullopt;
    }
    struct Inflater {
        z_stream& stream;
        ~Inflater() { inflateEnd(&stream); }
    } inflater{stream};
    stream.next_in = reinterpret_cast<Bytef*>(compressed.data());
    stream.avail_in = static_cast<uInt>(compressed.size());

    std::string scanlines;
    std::array<char, 64 * 1024> buffer{};
    int status;
    do {
        stream.next_out = reinterpret_cast<Bytef*>(buffer.data());
        stream.avail_out = static_cast<uInt>(buffer.size());
        status = inflate(&stream, Z_NO_FLUSH);
        if (status != Z_OK && status != Z_STREAM_END) {
            return std::nullopt;
        }
        auto const produced = buffer.size() - stream.avail_out;
        if (produced > scanline_limit - scanlines.size()) {
            return std::nullopt;
        }
        scanlines.append(buffer.data(), produced);
    } while (status != Z_STREAM_END);

    if (stream.avail_in != 0 || scanlines.empty()) {
        return std::nullopt;
    }
    return scanlines;
}

} // namespace

char const* png_codec_version() {
    return zlibVersion();
}

std::optional<std::string> compress_png(std::string_view source) {
    if (!source.starts_with(png_signature) || source.size() > image_byte_limit) {
        return std::nullopt;
    }

    // Recompress only the IDAT zlib stream. Keeping the original scanlines and
    // surrounding chunks preserves bit depth, palette, alpha, and color metadata.
    std::string compressed;
    std::size_t idat_start = 0;
    std::size_t idat_end = 0;
    bool ended = false;
    for (auto offset = png_signature.size(); source.size() - offset >= chunk_overhead;) {
        auto const size = read_u32(source.substr(offset));
        if (size > source.size() - offset - chunk_overhead) {
            return std::nullopt;
        }
        auto const type_and_data = source.substr(offset + 4, size + 4);
        auto const type = type_and_data.substr(0, 4);
        auto const data = type_and_data.substr(4);
        auto const crc_offset = offset + 8 + size;
        auto const next = crc_offset + 4;
        if (chunk_crc(type_and_data) != read_u32(source.substr(crc_offset)) || type == "acTL") {
            return std::nullopt;
        }
        if (offset == png_signature.size()) {
            if (type != "IHDR" || size != 13) {
                return std::nullopt;
            }
            auto const width = read_u32(data);
            auto const height = read_u32(data.substr(4));
            if (width == 0 || height == 0 || width > image_pixel_limit / height) {
                return std::nullopt;
            }
        }
        if (type == "IDAT") {
            if (idat_start == 0) {
                idat_start = offset;
            } else if (idat_end != offset) {
                return std::nullopt;
            }
            compressed.append(data);
            idat_end = next;
        }
        if (type == "IEND") {
            ended = size == 0 && next == source.size();
            break;
        }
        offset = next;
    }
    if (!ended || compressed.empty()) {
        return std::nullopt;
    }

    auto const scanlines = inflate_scanlines(compressed);
    if (!scanlines) {
        return std::nullopt;
    }
    auto size = static_cast<uLongf>(idat_end - idat_start - chunk_overhead);
    std::string encoded(size, '\0');
    if (compress2(reinterpret_cast<Bytef*>(encoded.data()), &size,
                  reinterpret_cast<Bytef const*>(scanlines->data()),
                  static_cast<uLong>(scanlines->size()), Z_BEST_COMPRESSION) != Z_OK ||
        size >= encoded.size()) {
        return std::nullopt;
    }
    encoded.resize(size);

    std::string result;
    result.reserve(source.size());
    result.append(source.substr(0, idat_start));
    append_u32(result, static_cast<std::uint32_t>(size));
    auto const crc_start = result.size();
    result.append("IDAT").append(encoded);
    append_u32(result, chunk_crc(std::string_view(result).substr(crc_start)));
    result.append(source.substr(idat_end));
    return result;
}

} // namespace mizuki
