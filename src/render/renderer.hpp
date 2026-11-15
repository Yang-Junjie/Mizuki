#pragma once
#include <filesystem>
#include <string>
#include <string_view>

namespace mizuki {

struct RenderValues {
    std::string_view asset_base;
    std::string_view title;
    std::string_view date;
};

class Renderer {
public:
    explicit Renderer(std::filesystem::path layout_path);

    bool render(std::string_view content, RenderValues const& values);

    std::string const& get_html() const;

private:
    std::filesystem::path layout_path_;
    std::string page_;
};
}
