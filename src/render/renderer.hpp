#pragma once
#include <filesystem>
#include <span>
#include <string>
#include <string_view>

namespace mizuki {

struct TemplateValue {
    std::string_view placeholder;
    std::string_view value;
    bool raw_html{false};
};

class Renderer {
public:
    explicit Renderer(std::filesystem::path layout_path);

    bool render(std::span<TemplateValue const> values);

    std::string const& get_html() const;

private:
    std::filesystem::path layout_path_;
    std::string page_;
};
}
