#include "build/image_codecs.hpp"

#include <cstddef>
#include <memory>
#include <string>
#include <turbojpeg.h>
#include <vector>

namespace mizuki {
namespace {

constexpr std::string_view jpeg_start{"\xff\xd8", 2};
constexpr std::string_view jpeg_end{"\xff\xd9", 2};

using JpegHandle = std::unique_ptr<void, decltype(&tj3Destroy)>;
using JpegBuffer = std::unique_ptr<unsigned char, decltype(&tj3Free)>;

std::size_t marker_length(std::string_view source, std::size_t length_offset) {
    return (static_cast<unsigned char>(source[length_offset]) << 8) |
           static_cast<unsigned char>(source[length_offset + 1]);
}

struct JpegMarkers {
    std::string jfif;
    std::string metadata;
};

std::optional<JpegMarkers> jpeg_markers(std::string_view source) {
    JpegMarkers markers;
    for (std::size_t offset = jpeg_start.size(); offset < source.size();) {
        auto const begin = offset;
        if (static_cast<unsigned char>(source[offset++]) != 0xff) {
            return std::nullopt;
        }
        while (offset < source.size() && static_cast<unsigned char>(source[offset]) == 0xff) {
            ++offset;
        }
        if (source.size() - offset < 3) {
            return std::nullopt;
        }
        auto const marker = static_cast<unsigned char>(source[offset++]);
        auto const length = marker_length(source, offset);
        if (length < 2 || length > source.size() - offset) {
            return std::nullopt;
        }
        if (marker == 0xda) {
            return markers;
        }
        auto const payload = source.substr(offset + 2, length - 2);
        offset += length;
        auto const segment = source.substr(begin, offset - begin);
        if (marker == 0xe0 && payload.starts_with(std::string_view{"JFIF\0", 5})) {
            if (!markers.jfif.empty() || payload.size() != 14) {
                return std::nullopt;
            }
            markers.jfif = segment;
        } else if ((marker == 0xe1 && payload.starts_with(std::string_view{"Exif\0\0", 6})) ||
                   (marker == 0xe2 && payload.starts_with(std::string_view{"ICC_PROFILE\0", 12})) ||
                   marker == 0xfe) {
            markers.metadata.append(segment);
        } else if (marker >= 0xe0 && marker <= 0xef) {
            // Unknown APP markers may refer to the original bitstream or contain extra images.
            return std::nullopt;
        }
    }
    return std::nullopt;
}

} // namespace

std::optional<std::string> compress_jpeg(std::string_view source, int quality) {
    if (!source.starts_with(jpeg_start) || !source.ends_with(jpeg_end) ||
        source.size() > image_byte_limit) {
        return std::nullopt;
    }
    auto const markers = jpeg_markers(source);
    if (!markers) {
        return std::nullopt;
    }

    JpegHandle decoder(tj3Init(TJINIT_DECOMPRESS), tj3Destroy);
    auto const* jpeg = reinterpret_cast<unsigned char const*>(source.data());
    if (!decoder || tj3Set(decoder.get(), TJPARAM_STOPONWARNING, 1) != 0 ||
        tj3DecompressHeader(decoder.get(), jpeg, source.size()) != 0 ||
        tj3Get(decoder.get(), TJPARAM_PRECISION) != 8 ||
        tj3Get(decoder.get(), TJPARAM_LOSSLESS) != 0) {
        return std::nullopt;
    }
    auto const width = tj3Get(decoder.get(), TJPARAM_JPEGWIDTH);
    auto const height = tj3Get(decoder.get(), TJPARAM_JPEGHEIGHT);
    auto const color_space = tj3Get(decoder.get(), TJPARAM_COLORSPACE);
    auto const subsampling = tj3Get(decoder.get(), TJPARAM_SUBSAMP);
    if (width <= 0 || height <= 0 ||
        static_cast<std::size_t>(width) > image_pixel_limit / static_cast<std::size_t>(height) ||
        (color_space != TJCS_YCbCr && color_space != TJCS_GRAY) || subsampling < 0) {
        return std::nullopt;
    }
    auto const format = color_space == TJCS_GRAY ? TJPF_GRAY : TJPF_RGB;
    auto const channels = format == TJPF_GRAY ? 1 : 3;
    std::vector<unsigned char> pixels(static_cast<std::size_t>(width) * height * channels);
    if (tj3Decompress8(decoder.get(), jpeg, source.size(), pixels.data(), 0, format) != 0) {
        return std::nullopt;
    }

    JpegHandle encoder(tj3Init(TJINIT_COMPRESS), tj3Destroy);
    if (!encoder || tj3Set(encoder.get(), TJPARAM_QUALITY, quality) != 0 ||
        tj3Set(encoder.get(), TJPARAM_SUBSAMP, subsampling) != 0 ||
        tj3Set(encoder.get(), TJPARAM_OPTIMIZE, 1) != 0) {
        return std::nullopt;
    }
    unsigned char* buffer = nullptr;
    std::size_t length = 0;
    auto const status =
        tj3Compress8(encoder.get(), pixels.data(), width, 0, height, format, &buffer, &length);
    JpegBuffer encoded(buffer, tj3Free);
    if (status != 0 || !encoded || length >= source.size()) {
        return std::nullopt;
    }

    std::string_view result(reinterpret_cast<char const*>(encoded.get()), length);
    auto const app0 = result.substr(2, 2) == std::string_view{"\xff\xe0", 2};
    auto const metadata_start = app0 ? 4 + marker_length(result, 4) : jpeg_start.size();
    std::string output(
        result.substr(0, markers->jfif.empty() ? metadata_start : jpeg_start.size()));
    output.append(markers->jfif).append(markers->metadata).append(result.substr(metadata_start));
    return output;
}

} // namespace mizuki
