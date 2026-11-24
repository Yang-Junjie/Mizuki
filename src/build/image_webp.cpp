#include "build/image_codecs.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <webp/decode.h>
#include <webp/encode.h>
#include <webp/mux.h>

namespace mizuki {

std::optional<std::string> compress_webp(std::string_view source, int quality) {
    if (source.size() > image_byte_limit) {
        return std::nullopt;
    }
    auto const* bytes = reinterpret_cast<std::uint8_t const*>(source.data());
    WebPBitstreamFeatures features{};
    if (WebPGetFeatures(bytes, source.size(), &features) != VP8_STATUS_OK ||
        features.has_animation || features.width <= 0 || features.height <= 0 ||
        static_cast<std::size_t>(features.width) >
            image_pixel_limit / static_cast<std::size_t>(features.height) ||
        (features.format != 1 && features.format != 2)) {
        return std::nullopt;
    }

    std::unique_ptr<std::uint8_t, decltype(&WebPFree)> pixels(
        WebPDecodeRGBA(bytes, source.size(), nullptr, nullptr), WebPFree);
    if (!pixels) {
        return std::nullopt;
    }
    WebPConfig config;
    if (!WebPConfigInit(&config)) {
        return std::nullopt;
    }
    config.lossless = features.format == 2;
    config.quality = config.lossless ? 75.0f : static_cast<float>(quality);
    config.exact = 1;
    if (!WebPValidateConfig(&config)) {
        return std::nullopt;
    }

    WebPPicture picture;
    if (!WebPPictureInit(&picture)) {
        return std::nullopt;
    }
    WebPMemoryWriter writer;
    WebPMemoryWriterInit(&writer);
    picture.width = features.width;
    picture.height = features.height;
    picture.use_argb = config.lossless;
    picture.writer = WebPMemoryWrite;
    picture.custom_ptr = &writer;
    bool const encoded = WebPPictureImportRGBA(&picture, pixels.get(), features.width * 4) &&
                         WebPEncode(&config, &picture);
    WebPPictureFree(&picture);
    std::unique_ptr<std::uint8_t, decltype(&WebPFree)> encoded_bytes(writer.mem, WebPFree);
    if (!encoded || writer.size >= source.size()) {
        return std::nullopt;
    }

    // Replace only the image in the original container, keeping its metadata.
    WebPData const original{bytes, source.size()};
    std::unique_ptr<WebPMux, decltype(&WebPMuxDelete)> mux(WebPMuxCreate(&original, 0),
                                                           WebPMuxDelete);
    WebPData const replacement{encoded_bytes.get(), writer.size};
    if (!mux || WebPMuxSetImage(mux.get(), &replacement, 0) != WEBP_MUX_OK) {
        return std::nullopt;
    }
    WebPData assembled{};
    auto const status = WebPMuxAssemble(mux.get(), &assembled);
    auto const clear = [](WebPData* data) { WebPDataClear(data); };
    std::unique_ptr<WebPData, decltype(clear)> output(&assembled, clear);
    if (status != WEBP_MUX_OK || !assembled.bytes || assembled.size >= source.size()) {
        return std::nullopt;
    }
    return std::string(reinterpret_cast<char const*>(assembled.bytes), assembled.size);
}

} // namespace mizuki
