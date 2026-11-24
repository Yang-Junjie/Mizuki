#include "build/images.hpp"

#include "build/image_codecs.hpp"
#include "util/file.hpp"
#include "util/path.hpp"

#include <spdlog/fmt/fmt.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cctype>
#include <jconfig.h>
#include <string>
#include <utility>
#include <webp/decode.h>
#include <webp/encode.h>
#include <webp/mux.h>

namespace mizuki {
namespace fs = std::filesystem;
namespace {

enum class Format { Png, Jpeg, Webp, Other };

Format format_of(fs::path const& path) {
    auto extension = path_to_utf8(path.extension());
    std::ranges::transform(extension, extension.begin(),
                           [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    if (extension == ".png") {
        return Format::Png;
    }
    if (extension == ".jpg" || extension == ".jpeg") {
        return Format::Jpeg;
    }
    return extension == ".webp" ? Format::Webp : Format::Other;
}

std::optional<std::string> compress(Format format, std::string_view source, int quality) {
    switch (format) {
    case Format::Png:
        return compress_png(source);
    case Format::Jpeg:
        return compress_jpeg(source, quality);
    case Format::Webp:
        return compress_webp(source, quality);
    case Format::Other:
        return std::nullopt;
    }
    return std::nullopt;
}

#define MIZUKI_STRINGIFY_DETAIL(x) #x
#define MIZUKI_STRINGIFY(x) MIZUKI_STRINGIFY_DETAIL(x)

std::string cache_key(Format format, std::string_view source, int quality) {
    auto const digest = sha256(source);
    switch (format) {
    case Format::Png:
        return fmt::format("v1-png-{}-{}", png_codec_version(), digest);
    case Format::Jpeg:
        return fmt::format("v1-jpeg-{}-{}-{}", MIZUKI_STRINGIFY(LIBJPEG_TURBO_VERSION), quality,
                           digest);
    case Format::Webp:
        return fmt::format("v1-webp-{}-{}-{}-{}-{}", WebPGetEncoderVersion(),
                           WebPGetDecoderVersion(), WebPGetMuxVersion(), quality, digest);
    case Format::Other:
        return {};
    }
    return {};
}

#undef MIZUKI_STRINGIFY
#undef MIZUKI_STRINGIFY_DETAIL

} // namespace

ImageWriter::ImageWriter(ImageOptions options, fs::path cache, fs::path stage)
    : options_(options), cache_(std::move(cache), stage) {}

void ImageWriter::write(fs::path const& source, fs::path const& destination) {
    auto const format = options_.compress ? format_of(source) : Format::Other;
    if (format == Format::Other || fs::file_size(source) > image_byte_limit) {
        fs::copy_file(source, destination);
        return;
    }

    auto const original = read_file(source);
    std::string key;
    std::optional<std::string> result;
    if (cache_.enabled() && !original.empty()) {
        key = cache_key(format, original, options_.quality);
        result = cache_.read(key, original.size() - 1);
    }

    if (result) {
        ++reused_;
    } else {
        result = compress(format, original, options_.quality);
        if (!result || result->size() >= original.size()) {
            result = std::string{};
        }
        if (!key.empty()) {
            cache_.write(key, *result);
        }
    }

    if (!result->empty()) {
        ++optimized_;
        bytes_saved_ += original.size() - result->size();
        write_file(destination, *result);
    } else {
        write_file(destination, original);
    }
}

void ImageWriter::log_summary() const {
    if (options_.compress) {
        spdlog::info("Images: {} optimized, {} cache hits, {} bytes saved", optimized_, reused_,
                     bytes_saved_);
    }
}

} // namespace mizuki
