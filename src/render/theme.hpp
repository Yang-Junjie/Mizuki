#pragma once

#include <filesystem>
#include <memory>
#include <nlohmann/json.hpp>
#include <string>
#include <string_view>

namespace mizuki {

// One instance per build: templates are cached within a consistent build snapshot.
class Theme {
public:
    explicit Theme(std::filesystem::path directory, std::string base_path = "/");
    ~Theme();
    Theme(Theme const&) = delete;
    Theme& operator=(Theme const&) = delete;

    nlohmann::json const& defaults() const;
    std::string render(std::string_view layout, nlohmann::json const& context);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace mizuki
