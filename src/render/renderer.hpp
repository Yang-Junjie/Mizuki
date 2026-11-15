#pragma once
#include <filesystem>
#include <string>
#include <string_view>

namespace mizuki {
class Renderer {
public:
    explicit Renderer(std::filesystem::path layout_path);

    bool render(std::string_view content);

    std::string const& get_html() const;

private:
    std::filesystem::path layout_path_;
    std::string page_;
};
}
