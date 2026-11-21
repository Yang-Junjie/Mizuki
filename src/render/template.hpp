#pragma once

#include <filesystem>
#include <span>
#include <string>
#include <string_view>

namespace mizuki {

enum class ValueKind { Text, Html };
enum class Requirement { Required, Optional };

struct TemplateValue {
    std::string_view placeholder;
    std::string_view value;
    ValueKind kind;
    Requirement requirement;

    static TemplateValue text(std::string_view placeholder, std::string_view value,
                              Requirement requirement = Requirement::Required) {
        return {placeholder, value, ValueKind::Text, requirement};
    }

    static TemplateValue html(std::string_view placeholder, std::string_view value,
                              Requirement requirement = Requirement::Required) {
        return {placeholder, value, ValueKind::Html, requirement};
    }
};

// A loaded template is reusable; rendering does not mutate it or re-read the file.
class Template {
public:
    explicit Template(std::filesystem::path path);

    std::string render(std::span<TemplateValue const> values) const;

private:
    std::filesystem::path path_;
    std::string source_;
};

} // namespace mizuki
