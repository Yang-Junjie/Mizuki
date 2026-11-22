#include "parse/front_matter.hpp"
#include "render/publication.hpp"
#include "render/template.hpp"
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

using mizuki::Template;
using mizuki::TemplateValue;
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

void loaded_template_is_reusable_and_renders_independent_values() {
    TemporaryDirectory temporary;
    auto const path = temporary.path() / "layout.html";
    mizuki::write_file(path, "<h1>{title}</h1>{body}");
    Template const layout(path);
    mizuki::write_file(path, "This edit belongs to the next build.");
    auto const first = std::array{
        TemplateValue::text("{title}", "<First>"),
        TemplateValue::html("{body}", "<p>{title}</p>"),
    };
    require(layout.render(first) == "<h1>&lt;First&gt;</h1><p>{title}</p>",
            "Template changed after loading or recursively replaced inserted HTML");
    auto const second = std::array{
        TemplateValue::text("{title}", "Second"),
        TemplateValue::html("{body}", ""),
    };
    require(layout.render(second) == "<h1>Second</h1>", "Rendering retained previous values");
}

void template_failure_has_context_and_does_not_poison_later_renders() {
    TemporaryDirectory temporary;
    auto const path = temporary.path() / "layout.html";
    mizuki::write_file(path, "{title}");
    Template const layout(path);
    auto const missing = std::array{TemplateValue::html("{markdown}", "body")};
    auto const error = expect_failure([&] { layout.render(missing); });
    require(error.find("{markdown}") != std::string::npos, "Error omitted missing placeholder");
    require(error.find(mizuki::path_to_utf8(path)) != std::string::npos,
            "Error omitted template path");
    auto const valid = std::array{TemplateValue::text("{title}", "Still usable")};
    require(layout.render(valid) == "Still usable", "Failed rendering poisoned the template");
    auto const absent = temporary.path() / "missing.html";
    auto const load_error = expect_failure([&] { Template const missing_layout(absent); });
    require(load_error.find(mizuki::path_to_utf8(absent)) != std::string::npos &&
                load_error.find("Failed to open file") != std::string::npos,
            "Template loading error omitted file context or underlying cause");
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
        loaded_template_is_reusable_and_renders_independent_values();
        template_failure_has_context_and_does_not_poison_later_renders();
        sitemap_enforces_single_file_limits();
    } catch (std::exception const& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
