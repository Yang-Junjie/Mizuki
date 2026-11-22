#include "scaffold/site.hpp"

#include "embedded_theme.hpp"
#include "scaffold/files.hpp"
#include "util/date.hpp"
#include "util/path.hpp"

#include <stdexcept>
#include <string_view>

namespace mizuki {
namespace fs = std::filesystem;
namespace {

constexpr std::string_view kConfig = R"(title = "Mizuki"
description = "我的博客"
author = ""
language = "zh-CN"
base_url = "/"
content = "content"
theme = "themes/default"
output = "dist"
paginate_by = 10
archives = true

[[navigation]]
title = "首页"
url = "/"

[[navigation]]
title = "关于"
url = "/pages/about.md"

[[navigation]]
title = "归档"
url = "/archives/"
)";

constexpr std::string_view kAbout = R"(---
title: 关于
---

欢迎来到我的博客。

你可以在 `mizuki.toml` 中设置站点名称、简介和导航。

[阅读示例文章](../posts/welcome.md) · [返回首页](/index.md)
)";

} // namespace

void init_site(fs::path const& directory) {
    if (directory.empty()) {
        throw std::runtime_error("Site directory must not be empty");
    }
    auto target = fs::absolute(directory);
    while (target.has_relative_path() && (target.filename().empty() || target.filename() == ".")) {
        target = target.parent_path();
    }
    auto const status = fs::symlink_status(target);
    if (fs::is_symlink(status) || (fs::exists(status) && !fs::is_directory(status))) {
        throw std::runtime_error("Site path must be a directory, not a file or symbolic link: " +
                                 path_to_utf8(target));
    }
    if (fs::exists(status) && !fs::is_empty(target)) {
        throw std::runtime_error("Site directory must be empty: " + path_to_utf8(target));
    }
    target = fs::weakly_canonical(target);
    auto const welcome = "---\ntitle: 欢迎使用 Mizuki\ndate: " + local_date() +
                         "\n---\n\n这是你的第一篇文章，可以直接修改这份 Markdown 文件。\n\n"
                         "[关于这个站点](../pages/about.md) · [返回首页](/index.md)\n";
    FileCreation files;
    files.write(target / "mizuki.toml", kConfig);
    files.write(target / "content/posts/welcome.md", welcome);
    files.write(target / "content/pages/about.md", kAbout);
    files.write(target / ".gitignore", "/dist/\n");
    for (auto const& file : kEmbeddedTheme) {
        files.write(target / "themes/default" / path_from_utf8(file.path), file.content);
    }
    files.commit();
}

} // namespace mizuki
