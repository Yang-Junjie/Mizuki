#pragma once

#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace mizuki {

struct ContentSummary {
    std::string title;
    std::string date;
    std::string url; // Unencoded output path, relative to the site root.
    std::string description;
    nlohmann::json extra = nlohmann::json::object();
    std::vector<std::string> tags;
    std::vector<std::string> categories;
};

} // namespace mizuki
