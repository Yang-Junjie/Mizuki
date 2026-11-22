#include "config/values.hpp"

#include <cmath>
#include <stdexcept>

namespace mizuki {

nlohmann::json template_value(toml::node const& node) {
    if (auto const* table = node.as_table()) {
        auto result = nlohmann::json::object();
        for (auto const& [key, value] : *table) {
            result[std::string(key.str())] = template_value(value);
        }
        return result;
    }
    if (auto const* array = node.as_array()) {
        auto result = nlohmann::json::array();
        for (auto const& value : *array) {
            result.push_back(template_value(value));
        }
        return result;
    }
    if (node.is_string()) {
        return *node.value<std::string>();
    }
    if (node.is_integer()) {
        return *node.value<std::int64_t>();
    }
    if (node.is_boolean()) {
        return *node.value<bool>();
    }
    if (auto const value = node.value<double>(); node.is_floating_point() && value) {
        if (!std::isfinite(*value)) {
            throw std::runtime_error("Extra parameters require finite numbers");
        }
        return *value;
    }
    throw std::runtime_error("Extra parameters support strings, booleans, numbers, arrays and "
                             "tables; quote TOML date/time values to use them as strings");
}

nlohmann::json merge_extra(nlohmann::json defaults, nlohmann::json const& overrides) {
    for (auto const& [key, value] : overrides.items()) {
        if (value.is_object() && defaults.contains(key) && defaults[key].is_object()) {
            defaults[key] = merge_extra(std::move(defaults[key]), value);
        } else {
            defaults[key] = value;
        }
    }
    return defaults;
}

} // namespace mizuki
