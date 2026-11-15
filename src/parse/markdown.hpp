#pragma once
#include <string>
#include <filesystem>
#include <md4c-html.h>
namespace mizuki{
class Markdown {
public:
    explicit Markdown(std::filesystem::path markdown_path);

    bool to_html();

    std::string const& get_html() const;
    std::string get_html_or(std::string fallback = "none");

private:
    static void on_html_output(const MD_CHAR* text, MD_SIZE size, void* userdata);

    std::filesystem::path markdown_path_;
    std::string html_;
};
}
