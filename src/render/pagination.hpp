#pragma once

#include <cstddef>
#include <string>
#include <string_view>

namespace mizuki {

std::string render_pagination(std::size_t current, std::size_t total, std::string_view previous,
                              std::string_view next);

} // namespace mizuki
