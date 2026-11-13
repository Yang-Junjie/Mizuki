#include <md4c-html.h>
#include <md4c.h>
#include <spdlog/spdlog.h>

#include <string>
#include <string_view>
#include <filesystem>
#include <fstream>

namespace {

constexpr std::string_view markdown = "# Hello\n\nThis is **Mizuki**.";

int on_enter_block(MD_BLOCKTYPE type, void*, void*) {
    spdlog::info("[md4c] enter block {}", static_cast<int>(type));
    return 0;
}

int on_leave_block(MD_BLOCKTYPE, void*, void*) { return 0; }
int on_enter_span(MD_SPANTYPE, void*, void*) { return 0; }
int on_leave_span(MD_SPANTYPE, void*, void*) { return 0; }

int on_text(MD_TEXTTYPE, const MD_CHAR* text, MD_SIZE size, void*) {
    spdlog::info("[md4c] text: {}", std::string_view(text, size));
    return 0;
}

void example_md4c() {
    const MD_PARSER parser{
        .enter_block = on_enter_block,
        .leave_block = on_leave_block,
        .enter_span = on_enter_span,
        .leave_span = on_leave_span,
        .text = on_text,
    };

    const int rc = md_parse(markdown.data(), static_cast<MD_SIZE>(markdown.size()), &parser, nullptr);
    if (rc != 0) {
        spdlog::error("md_parse failed: {}", rc);
    }
}

void on_html_output(const MD_CHAR* text, MD_SIZE size, void* userdata) {
    static_cast<std::string*>(userdata)->append(text, size);
}

void example_md4c_html() {
    std::string html;
    const int rc = md_html(markdown.data(), static_cast<MD_SIZE>(markdown.size()),
                           on_html_output, &html, MD_DIALECT_GITHUB, 0);
    if (rc != 0) {
        spdlog::error("md_html failed: {}", rc);
        return;
    }

    spdlog::info("[md4c-html] result:\n{}", html);
}

}  // namespace

std::string readFile(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    std::ostringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

int main(int argc, char** args) {
    example_md4c();
    example_md4c_html();

    if (argc < 2) {
        spdlog::error("No arguments provided");
        return 1;
    }

    std::filesystem::path markdown_path(args[1]);
    std::ifstream markdown_file(markdown_path.string());
    if (!markdown_file.is_open()) {
        spdlog::error("Failed to open markdown file: {}", markdown_path.string());
        return 1;
    }

    std::string markdown_content = readFile(markdown_path.string());

    spdlog::info("Read markdown content:\n{}", markdown_content);

    std::string html;
    const int rc = md_html(markdown_content.data(), static_cast<MD_SIZE>(markdown_content.size()),
                           on_html_output, &html, MD_DIALECT_GITHUB, 0);
    if (rc != 0) {
        spdlog::error("md_html failed: {}", rc);
        return 1;
    }

    spdlog::info("[md4c-html] result:\n{}", html);
    std::ofstream html_file("output.html", std::ios::binary);
    if (!html_file.is_open()) {
        spdlog::error("Failed to open HTML output file");
        return 1;
    }
    html_file.write(html.data(), static_cast<std::streamsize>(html.size()));
    html_file.close();

    return 0;
}
