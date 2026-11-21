#pragma once
#include "parse/front_matter.hpp"

#include <filesystem>
#include <md4c-html.h>
#include <string>

namespace mizuki {
class Markdown {
public:
    explicit Markdown(std::filesystem::path markdown_path, bool require_date = true);

    bool load();
    bool to_html();

    FrontMatter const& front_matter() const;
    std::string const& get_html() const;
    std::string get_html_or(std::string fallback = "none");
    std::string const& error() const;

private:
    static void on_html_output(const MD_CHAR* text, MD_SIZE size, void* userdata);

    std::filesystem::path markdown_path_;
    std::string body_;
    std::string html_;
    FrontMatter front_matter_;
    std::string error_;
    bool loaded_{false};
    bool require_date_{true};
};
}
