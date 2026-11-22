---
title: 快速开始
description: 创建一个可编辑、可预览、可发布的 Mizuki 站点。
---

Mizuki 把 Markdown、TOML 配置和主题模板构建为 HTML、CSS 与资源文件。本地预览时运行 Mizuki；发布后，服务器只需提供生成的静态文件。本文先完成一个博客，再说明本站文档的构建方式。

## 安装与启动

可以从 [GitHub Releases](https://github.com/Yang-Junjie/Mizuki/releases) 下载 Linux 或 Windows x86_64 发布包，解压后将 `bin/` 加入 `PATH`。Linux 发布包的基线是 Ubuntu 24.04；其他平台可以尝试从源码构建。

```sh
mizuki --version
mizuki init my-blog
cd my-blog
mizuki serve
```

打开 `http://127.0.0.1:3939/`。编辑文章、主题或配置，Mizuki 会重新构建，成功后刷新浏览器。构建失败时会记录原因，保留上次成功的网页；修复后继续工作。

`init` 需要不存在或为空的目标目录，会创建配置、示例内容与完整默认主题。初始化后的站点不依赖 Mizuki 源码目录，所有主题文件都可以直接编辑。

### 从源码编译

需要 CMake 3.28 或更新版本、支持 C++23 的编译器，以及首次下载依赖时的网络连接。运行测试还需要 Python 3。

```sh
git clone https://github.com/Yang-Junjie/Mizuki.git
cd Mizuki
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
./build/bin/mizuki --version
```

Windows 使用 MSVC 开发者终端与 Ninja 构建时，程序为 `build/bin/mizuki.exe`；Visual Studio 多配置生成器需要增加 `--config Release`，程序位于 `build/bin/Release/mizuki.exe`。

后文的 `mizuki` 表示这个可执行文件。可以将它加入 `PATH`，也可以始终使用绝对路径。

## 认识站点目录

```text
my-blog/
  mizuki.toml
  content/
    posts/hello.md
    pages/about.md
  themes/default/
    theme.toml
    layouts/
    assets/
  dist/                 # 构建后生成
```

`content/` 中的 Markdown 按原有目录生成 `.html`。`pages/` 下的是独立页面，不进入文章列表；其他 Markdown 是文章，需要日期。其他文件作为静态资源复制。根级 `index.md` 保留给生成首页，正文不会用来替换首页；首页由主题的 `home.html` 定义。

`themes/default/layouts/` 决定页面结构，`assets/` 放 CSS、图片与其他主题资源。`dist/` 是构建产物，下一次成功构建会整体替换它，需发布的文件应该放在内容或主题资源目录中。

## 写第一篇文章

在站点目录中创建文稿：

```sh
mizuki new posts/my-first-post.md --title "我的第一篇文章"
```

打开生成文件，编辑元数据和正文：

```md
---
title: 我的第一篇文章
date: 2026-11-22
description: 记录建站的第一步。
draft: false
---

这是正文。

## 开始写作

使用段落、列表、代码块和链接组织内容。
```

front matter 是简单的单行键值格式，支持 `title`、`date`、`description`、`draft` 与 `layout`，不是完整 YAML。文章要求非空标题与有效的 `YYYY-MM-DD` 日期；独立页面可以省略日期。`draft: true` 的内容不发布，日期本身不承担定时发布功能。

需要先写草稿时：

```sh
mizuki new posts/work-in-progress.md --title "写作中" --draft
```

`new` 不覆盖已有文件，也不接受越出内容目录的路径。准备发布后，将 `draft` 改为 `false`。

### 独立页面与站内链接

```sh
mizuki new pages/projects.md --title "我的项目"
```

在正文中按 Markdown 源文件路径链接：

```md
[关于](/pages/about.md)
[上一篇](../posts/hello.md)
[回到首页](/index.md)
```

Mizuki 会把 `.md` 转换为实际发布的 `.html`，保留查询参数和章节锚点。缺失、草稿或越出内容目录的 Markdown 目标会导致构建失败。图片可以放在 `content/images/` 中，使用 `![说明](/images/example.png)` 引用。

## 配置站点

编辑 `mizuki.toml`：

```toml
title = "我的博客"
description = "写作、项目与日常记录"
author = "作者"
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

[extra]
show_toc = true
footer_text = "感谢阅读"
```

配置中的目录相对配置文件解析。`paginate_by = 0` 关闭分页；`archives = true` 生成按年份分组的归档。导航必须指向已生成的本地页面或外部地址。

`[extra]` 是主题参数。默认主题支持目录开关与页脚文字；自定义主题可以增加自己的参数，详见[主题开发](themes.md)。站点配置的未知根级字段会报错，主题自由参数应放在 `[extra]` 中。

## 构建与预览

```sh
mizuki build
mizuki watch
mizuki serve --port 4000
```

- `build` 执行一次构建，成功后输出可以直接部署。
- `watch` 监听变化并重建，适合自行配合其他 HTTP 服务。
- `serve` 同时提供本地 HTTP 预览与浏览器自动刷新。

三种命令都支持 `--config`、`--content`、`--theme`、`--output` 与 `--base-url`。命令行值覆盖配置文件；命令行中的相对目录按当前工作目录解析。

```sh
mizuki serve --config path/to/mizuki.toml
mizuki build --base-url https://example.com/blog/
```

正式发布建议提供完整 HTTP(S) 地址，以生成 canonical、RSS 与 sitemap。部署子路径及 GitHub Actions 配置见[部署指南](deployment.md)。

## 构建这个文档站

本站没有使用其他文档生成器。仓库中的 `docs/mizuki.toml` 指向 Markdown 正文和一个独立的 Inja 文档主题：

```text
docs/
  mizuki.toml
  content/pages/         # 本文和其他指南
  theme/layouts/        # 首页、文档布局和组件
  theme/assets/         # 本站 CSS
```

在 Mizuki 仓库根目录中运行：

```sh
./build/bin/mizuki serve --config docs/mizuki.toml
./build/bin/mizuki build --config docs/mizuki.toml
```

本地输出为 `build/docs/`。GitHub Pages 工作流从源码编译同一个程序，再用这份配置构建并上传网页。想理解站点如何生成，可以接着阅读[架构设计](architecture.md)；想修改它的样式，可以直接参考[本站主题源码](https://github.com/Yang-Junjie/Mizuki/tree/main/docs/theme)。
