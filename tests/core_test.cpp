#include "parse/front_matter.hpp"
#include "parse/markdown.hpp"
#include "render/publication.hpp"
#include "render/theme.hpp"
#include "scaffold/files.hpp"
#include "support.hpp"
#include "util/file.hpp"
#include "util/path.hpp"

#include <array>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using mizuki::Theme;
using mizuki::test::require;
using mizuki::test::TemporaryDirectory;

template <class Action> std::string expect_failure(Action action) {
    try {
        action();
    } catch (std::exception const& error) {
        return error.what();
    }
    throw std::runtime_error("Expected an exception");
}

void document_owns_its_metadata_and_body() {
    auto document = [] {
        std::string source = "---\ntitle: Owned title\ndate: 2026-11-22\ndescription: Owned "
                             "summary\n---\nOwned body.\n";
        auto parsed = mizuki::parse_document(source, mizuki::DocumentKind::Article);
        source.assign(source.size(), 'x');
        return parsed;
    }();
    require(document.metadata.title == "Owned title", "Parsed title depends on source lifetime");
    require(document.metadata.date == "2026-11-22", "Parsed date depends on source lifetime");
    require(document.metadata.description == "Owned summary",
            "Parsed summary depends on source lifetime");
    require(document.body == "Owned body.\n", "Parsed body depends on source lifetime");
}

void rendered_markdown_owns_its_html_and_heading_metadata() {
    auto rendered = [] {
        std::string source = "## *安装* 与 `C++`\n\n### 配置\n";
        auto result = mizuki::render_markdown(source);
        source.assign(source.size(), 'x');
        return result;
    }();
    require(rendered.headings.size() == 2, "Heading metadata omitted a heading");
    auto const& heading = rendered.headings.front();
    require(heading.level == 2 && heading.id == "安装-与-c" && heading.text == "安装 与 C++",
            "Heading metadata lost its level, ID or plain text");
    require(rendered.html.find("<em>安装</em> 与 <code>C++</code>") != std::string::npos,
            "Adding anchors changed heading formatting");
    auto const second = mizuki::render_markdown("## 安装\n");
    require(second.headings.front().id == "安装", "Heading IDs leaked between documents");
}

void loaded_template_is_reusable_and_renders_independent_values() {
    TemporaryDirectory temporary;
    auto const path = temporary.path() / "layouts/layout.html";
    std::filesystem::create_directory(path.parent_path());
    mizuki::write_file(path, "<h1>{{ title }}</h1>{{ html(body) }}");
    Theme theme(temporary.path());
    auto const first =
        theme.render("layout.html", {{"title", "<First>"}, {"body", "<p>{{ title }}</p>"}});
    mizuki::write_file(path, "This edit belongs to the next build.");
    require(first == "<h1>&lt;First&gt;</h1><p>{{ title }}</p>",
            "Template escaped HTML or recursively evaluated inserted content");
    require(theme.render("layout.html", {{"title", "Second"}, {"body", ""}}) == "<h1>Second</h1>",
            "Template was reloaded or retained previous values");
}

void template_failure_has_context_and_does_not_poison_later_renders() {
    TemporaryDirectory temporary;
    auto const path = temporary.path() / "layouts/layout.html";
    std::filesystem::create_directory(path.parent_path());
    mizuki::write_file(path, "{{ title }}");
    Theme theme(temporary.path());
    auto const error =
        expect_failure([&] { theme.render("layout.html", nlohmann::json::object()); });
    require(error.find("title") != std::string::npos, "Error omitted missing variable");
    require(error.find(mizuki::path_to_utf8(path)) != std::string::npos,
            "Error omitted template path");
    require(theme.render("layout.html", {{"title", "Still usable"}}) == "Still usable",
            "Failed rendering poisoned the template");
    auto const load_error = expect_failure([&] { theme.render("missing.html", {}); });
    require(load_error.find("missing.html") != std::string::npos &&
                load_error.find("Failed to open file") != std::string::npos,
            "Template loading error omitted file context or underlying cause");
}

void creation_failure_removes_new_paths_and_preserves_existing_files() {
    TemporaryDirectory temporary;
    auto const root = temporary.path();
    auto const existing = root / "existing";
    std::filesystem::create_directory(existing);
    mizuki::write_file(existing / "keep.md", "User content");
    auto const error = expect_failure([&] {
        mizuki::FileCreation files;
        files.write(existing / "nested/post.md", "New article");
        files.write(root / "new-site/content/post.md", "New site");
        files.write(existing / "keep.md", "Would overwrite user content");
    });
    require(error.find("keep.md") != std::string::npos, "Creation failure omitted the file path");
    require(mizuki::read_file(existing / "keep.md") == "User content",
            "Exclusive creation overwrote an existing file");
    require(std::filesystem::is_directory(existing), "Rollback removed an existing directory");
    require(!std::filesystem::exists(existing / "nested"), "Rollback left new content directories");
    require(!std::filesystem::exists(root / "new-site"), "Rollback left an incomplete new site");
}

void creation_failure_does_not_remove_unrelated_new_files() {
    TemporaryDirectory temporary;
    auto const directory = temporary.path() / "shared";
    {
        mizuki::FileCreation files;
        files.write(directory / "owned.md", "Owned by this operation");
        mizuki::write_file(directory / "unrelated.md", "Created by someone else");
    }
    require(!std::filesystem::exists(directory / "owned.md"), "Uncommitted creation was retained");
    require(mizuki::read_file(directory / "unrelated.md") == "Created by someone else",
            "Rollback deleted an unrelated file");
}

void sitemap_enforces_single_file_limits() {
    std::vector<std::string> urls(50000, "https://example.com/");
    require(!mizuki::render_sitemap(urls).empty(), "A sitemap at the URL count limit failed");
    urls.emplace_back("https://example.com/extra");
    auto const count_error = expect_failure([&] { mizuki::render_sitemap(urls); });
    require(count_error.find("50000 URLs") != std::string::npos,
            "Sitemap URL count limit was not reported");
    urls.assign(26000, "https://example.com/" + std::string(2020, 'a'));
    auto const size_error = expect_failure([&] { mizuki::render_sitemap(urls); });
    require(size_error.find("50 MiB") != std::string::npos, "Sitemap size limit was not reported");
}

} // namespace

int main() {
    try {
        document_owns_its_metadata_and_body();
        rendered_markdown_owns_its_html_and_heading_metadata();
        loaded_template_is_reusable_and_renders_independent_values();
        template_failure_has_context_and_does_not_poison_later_renders();
        sitemap_enforces_single_file_limits();
        creation_failure_removes_new_paths_and_preserves_existing_files();
        creation_failure_does_not_remove_unrelated_new_files();
    } catch (std::exception const& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
