+++
title = "快速开始"
description = "创建一个可编辑、可预览、可发布的 Mizuki 站点。"
+++

Mizuki 把 Markdown、TOML 配置和主题模板构建为 HTML、CSS 与资源文件。本地预览时运行 Mizuki；发布后，服务器只需提供生成的静态文件。本文先完成一个博客；完整字段与参数可以继续查阅[站点配置](configuration.md)、[命令行](cli.md)和[内容与链接](content.md)。

## 安装与启动

从 [GitHub Releases](https://github.com/Yang-Junjie/Mizuki/releases) 下载对应系统的压缩包。日常建站无需安装编译器、CMake、Git 或 Python；默认主题已包含在程序中。

| 系统 | 压缩包 | 发布 CI 的构建与验证基线 |
| --- | --- | --- |
| Linux x86_64 | `mizuki-VERSION-linux-x86_64.tar.gz` | Ubuntu 24.04，glibc 2.39 或更新版本 |
| Windows x86_64 | `mizuki-VERSION-windows-x86_64.zip` | Windows Server 2022，MSVC |

Linux 包静态链接第三方库和 C++ 运行库，仍使用系统的 glibc。Windows 包静态链接第三方库和 MSVC 运行库。其他系统、架构以及 Windows 桌面版本尚未单独验证。

解压后，将其中的 `bin/` 加入当前终端的 `PATH`。Linux shell：

```sh
export PATH="/path/to/mizuki/bin:$PATH"
```

Windows PowerShell：

```powershell
$env:Path = "C:\Tools\mizuki\bin;$env:Path"
```

每个压缩包附带同名 `.sha256` 文件。Linux 可运行 `sha256sum --check 压缩包文件名.sha256`；Windows 用 `Get-FileHash 压缩包文件名 -Algorithm SHA256`，与文件中的摘要比较。对应源码和依赖位于同一 Release 的 `mizuki-VERSION-source.tar.gz`；许可证和依赖声明随程序放在 `share/mizuki/`。

安装后，创建并启动站点：

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

后文的 `mizuki` 表示这个可执行文件。可以将它加入 `PATH`，也可以始终使用绝对路径。依赖选择、CMake presets 和安装命令见[参与贡献](contributing.md#从源码构建)。

## 认识站点目录

```text
my-blog/
  mizuki.toml
  content/
    posts/welcome.md
    pages/about.md
  themes/default/
    theme.toml
    layouts/
    assets/
  dist/                 # 构建后生成
```

`content/` 中的 Markdown 按原有目录生成 `.html`。`pages/` 下的是独立页面，不进入文章列表；其他 Markdown 是文章，需要日期。其他文件作为静态资源复制。根级 `index.md` 保留给生成首页，正文不会用来替换首页；首页由主题的 `home.html` 定义。

`themes/default/layouts/` 决定页面结构，`assets/` 放 CSS、图片与其他主题资源。`dist/` 是构建产物，下一次成功构建会整体替换它，需发布的文件应该放在内容或主题资源目录中。

### 增量构建

在 `mizuki.toml` 中启用增量构建：

```toml
[build]
incremental = true
```

第一次构建生成完整站点并保存 Markdown 渲染结果和页面 HTML。后续构建根据正文、渲染选项、页面数据和模板依赖复用缓存。图片压缩可以通过同一配置下的 `[build.images]` 独立启用。

每次构建仍检查内部链接并生成完整的输出计划。删除文章、改为草稿或减少分页后，旧文件会从输出目录移除。产物完全相同时，Mizuki 跳过目录替换，本地预览也跳过这次页面刷新。

渲染缓存位于输出目录同级的 `.cache/mizuki/render/`。删除缓存可以重新渲染，缓存损坏时也会自动恢复。日志会分别列出 Markdown 和 HTML 的重渲染、复用数量。

### 构建时压缩图片

在 `mizuki.toml` 中启用图片压缩：

```toml
[build.images]
compress = true
quality = 85
```

`compress` 默认关闭。`quality` 默认为 85，接受 1–100，只用于 JPEG 和有损 WebP。PNG 无损重压缩，静态无损 WebP 保持无损。动画 PNG、动画 WebP、无法编码的图片和压缩后没有变小的图片仍按原样发布。

内容目录和主题中的图片都可以优化。源文件、输出格式和链接保持原样。Mizuki 在输出目录所在的父目录下使用 `.cache/mizuki/images/` 缓存处理结果。删除该缓存后可重新编码。构建日志会报告优化的图片数量、缓存命中数和节省的字节数。

## 写第一篇文章

在站点目录中创建文稿：

```sh
mizuki new posts/my-first-post.md --title "我的第一篇文章"
```

打开生成文件，编辑元数据和正文：

```md
+++
title = "我的第一篇文章"
date = "2026-11-22"
description = "记录建站的第一步。"
draft = false
tags = ["建站", "Mizuki"]
categories = ["笔记"]
+++

这是正文。

## 开始写作

使用段落、列表、代码块和链接组织内容。
```

`init` 与 `new` 生成由 `+++` 包裹的 TOML front matter。内置字段为 `title`、`date`、`description`、`draft`、`layout`、`tags` 与 `categories`；自定义数据放在 `[extra]` 中，可使用数组与嵌套表。文章要求非空标题与有效的 `YYYY-MM-DD` 日期；日期可以使用带引号的字符串或 TOML 本地日期，独立页面可以省略日期。`draft = true` 的内容不发布，日期本身不承担定时发布功能。

例如为当前页面关闭目录并设置封面图，在结束的 `+++` 前添加：

```toml
[extra]
show_toc = false
cover = "images/cover.webp"
cover_alt = "山间的晨光"
```

图片放在 `content/images/cover.webp`。默认主题会在详情页、首页列表和归档中显示封面，并按部署路径生成图片地址。页面目录设置优先于站点 `[extra].show_toc`；更多数据用法见[主题开发](themes.md#页面自定义数据)。已有的 `---` 简单键值文稿仍可构建；需要数组、嵌套数据或 TOML 注释时，请将开头和结尾改为 `+++`，并把字段改写为 TOML。

文章可以有多个标签和分类，分别填写根级 `tags`、`categories` 字符串数组。默认主题在文章中显示链接，首页提供总览入口，并自动生成对应列表和分页；草稿不参与聚合。分类为扁平结构，独立页面不填写这两项。名称和路径规则见[技术写作](writing.md#标签与分类)。

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
[欢迎文章](../posts/welcome.md)
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

完整字段、内置默认值与校验规则见[站点配置](configuration.md)。配置中的目录相对配置文件解析。`paginate_by = 0` 关闭分页；`archives = true` 生成按年份分组的归档。导航必须指向已生成的本地页面或外部地址。

`[markdown]` 控制数学、脚注与提示块语法，代码、公式及其样式示例见[技术写作](writing.md)。

`[extra]` 是主题参数。默认主题支持目录、代码高亮、复制、公式排版开关与页脚文字；自定义主题可以增加自己的参数，详见[主题开发](themes.md)。站点配置的未知根级字段会报错，主题自由参数应放在 `[extra]` 中。

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

## 预览这个文档站

本站也由 Mizuki 构建。完成源码编译后，在仓库根目录运行：

```sh
./build/bin/mizuki serve --config docs/mizuki.toml
```

正文位于 `docs/content/pages/`，主题位于 `docs/theme/`。新增文档、导航、资源与 Pages 部署的方法见[文档维护](documentation.md)。
