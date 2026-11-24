#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

namespace mizuki {

constexpr std::size_t image_byte_limit = 64 * 1024 * 1024;
constexpr std::size_t image_pixel_limit = 32 * 1024 * 1024;

char const* png_codec_version();
std::optional<std::string> compress_png(std::string_view source);
std::optional<std::string> compress_jpeg(std::string_view source, int quality);
std::optional<std::string> compress_webp(std::string_view source, int quality);

} // namespace mizuki
