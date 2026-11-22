#pragma once

#include <toml++/toml.hpp>

#include <nlohmann/json.hpp>

namespace mizuki {

// Only values with an unambiguous template representation are accepted.
nlohmann::json template_value(toml::node const& node);

// Tables merge recursively; arrays and scalar values are replaced as a whole.
nlohmann::json merge_extra(nlohmann::json defaults, nlohmann::json const& overrides);

} // namespace mizuki
