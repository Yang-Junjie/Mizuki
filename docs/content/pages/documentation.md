+++
title = "文档维护"
description = "修改、预览和验证由 Mizuki 自身构建的文档站，维护导航、资源与部署。"
+++

本文档站由仓库中的 Mizuki 构建，也是框架的实际使用示例。所有指南正文放在 `docs/content/pages/`，GitHub 中的 Markdown 与发布后的网页共用同一份内容。

## 文件分工

| 位置 | 职责 |
| --- | --- |
| `README.md` | 项目介绍、快速启动与文档入口 |
| `CONTRIBUTING.md` | GitHub 可发现的贡献入口 |
| `CHANGELOG.md` | 版本变化；发布工作流从对应版本章节提取 Release 说明 |
| `docs/mizuki.toml` | 文档站配置、侧栏导航与仓库链接 |
| `docs/content/pages/` | 使用、主题开发、架构与维护指南正文 |
| `docs/theme/layouts/` | 首页、文档布局与组件 |
| `docs/theme/assets/` | 样式、脚本、图片与本地字体 |
| `docs/theme/ASSETS.md` | 文档主题图片和字体的来源说明 |
| `examples/minimal-theme/` | 可直接构建的最小主题示例 |

根目录只保留项目入口、更新记录和许可证等必要文件。详细使用与维护说明在文档站中维护，避免在多个地方重复更新。

## 本地预览

下面的命令均在 Mizuki 仓库根目录运行。先编译程序；工具链与完整开发检查见[参与贡献](contributing.md)。

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
cmake --build build --parallel 2
./build/bin/mizuki serve --config docs/mizuki.toml
```

打开 `http://127.0.0.1:3939/`。正文、主题和配置变化都会触发重建，成功后刷新页面。Windows 使用对应构建目录中的 `mizuki.exe`。

只生成网页，或模拟项目子路径：

```sh
./build/bin/mizuki build --config docs/mizuki.toml
./build/bin/mizuki serve --config docs/mizuki.toml --base-url /Mizuki/
```

第二条命令的预览地址为 `http://127.0.0.1:3939/Mizuki/`。输出目录默认是 `build/docs/`，由 Mizuki 管理；修改正文和资源时应编辑源文件。

## 新增与修改指南

在 `docs/content/pages/` 添加 `.md` 文件，并提供 TOML front matter：

```md
+++
title = "新指南"
description = "说明这篇指南解决什么问题。"
+++

这里是正文。

## 第一个章节

使用相对链接引用其他指南，例如 [主题开发](themes.md)。
```

独立页面可以省略日期，正文从二级标题开始。页面标题由布局输出，`description` 用于首页卡片和页面描述。

- 首页通过 `site.pages` 自动收集已发布的独立页面，按输出路径升序排列，无需逐个修改首页模板。
- 需要出现在侧栏时，在 `docs/mizuki.toml` 的 `[[navigation]]` 中添加标题和 `/pages/新文件.md` 地址。
- 指向其他指南时使用相对 `.md` 链接，由 Mizuki 检查目标并改写为 `.html`；需要引用源码时使用仓库链接。
- 避免在站点正文中使用越出 `docs/content/` 的本地链接，例如 `../../../CONTRIBUTING.md`，它不是可发布的页面。
- 如需单独覆盖首页卡片说明，可在 `[extra.guide_descriptions]` 中以页面标题作为键；通常直接编辑文稿的 `description` 即可。

自编文档使用中文，命令和代码标识按实际接口书写。调整标题后，请同时检查其他页面指向该章节的锚点。

## 主题与资源

文档主题使用[主题开发](themes.md)中说明的同一套 Inja 接口。本站主题源码位于 `docs/theme/`，想从更少代码理解接口，可以使用[最小主题示例](https://github.com/Yang-Junjie/Mizuki/tree/main/examples/minimal-theme)。

- `assets/css/docs.css` 控制本站配色、字体、排版和组件外观。
- `assets/css/reading.css` 与默认主题共享阅读样式，本站覆盖样式随后加载。
- `assets/images/logo.webp` 是文档站与仓库 README 共用的标志。
- 图片、字体、脚本及许可证随站点发布；来源见[资源说明](https://github.com/Yang-Junjie/Mizuki/blob/main/docs/theme/ASSETS.md)。

共享阅读文件与第三方资源的更新方法见[参与贡献](contributing.md#技术写作与本地资源)。

## 验证文档

```sh
ctest --test-dir build -R '^mizuki\.docs$' --output-on-failure
```

该检查使用真实文档内容构建根路径、仓库子路径与含中文和空格的部署路径，核对页面、导航、正文链接、章节锚点、资源和 sitemap。新增或移除指南时，也要同步更新 `tests/test_docs.py` 中的预期页面清单。

迁移根目录文件时，还需检查 `cmake/packaging.cmake` 和 `tests/test_package.py` 的发布包清单，以及 `.github/workflows/` 是否读取原文件。`CHANGELOG.md` 保留在根目录，因为 Release 工作流会从中读取版本说明。

## 部署文档站

`.github/workflows/pages.yml` 在推送到 `main` 或手动触发时，从当前源码编译 Mizuki，验证并构建本站，再发布到 GitHub Pages。工作流使用 Pages 返回的地址覆盖 `base_url`，主题无需硬编码仓库前缀。

GitHub Pages 设置与故障排查见[部署指南](deployment.md#本仓库的文档站)。
