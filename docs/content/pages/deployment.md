---
title: 部署指南
description: 将 Mizuki 生成的静态网页部署到 GitHub Pages，正确处理域名与部署子路径。
---

站点发布地址通过 `mizuki.toml` 的 `base_url` 设置，也可以使用 `--base-url` 临时覆盖。构建输出由静态文件组成，不需要在发布服务器上运行 Mizuki。

## 先在本地验证

假设站点地址是 `https://example.com/blog/`：

```toml
base_url = "https://example.com/blog/"
```

```sh
./build/bin/mizuki serve
# 打开 http://127.0.0.1:3939/blog/
./build/bin/mizuki build
```

检查首页、深层文章、独立页面、图片和下载链接，然后把 `dist/` 中的内容发布到该地址。不要再将整个输出包入 `blog/` 目录。部署到域名根目录时使用 `https://example.com/` 或 `/`。

使用完整域名构建时，也会生成 `dist/rss.xml` 和 `dist/sitemap.xml`，默认主题加入对应的 RSS 入口与页面 canonical。仅使用 `/blog/` 模拟子路径时跳过这些发布信息。配置的域名始终是正式发布域名，本地预览仍监听本机地址；在预览中点击 RSS 入口会访问配置的正式地址。

文章中仍写 `[关于](/pages/about.md)`、`![图片](/images/photo.png)`，无需手动加入 `/blog/`；相对链接也可继续使用。自定义主题使用 `{{ asset_base }}` 引用主题资源，使用 `{{ site.base_path }}` 引用其他站点资源。原样复制的 HTML、CSS 和 `srcset` 请使用适当的相对 URL。

## 本仓库的文档站

Mizuki 仓库已经提供实际使用的 [Pages 工作流](https://github.com/Yang-Junjie/Mizuki/blob/main/.github/workflows/pages.yml)。它部署的就是正在阅读的文档站，流程如下：

1. 从当前提交编译 Mizuki，无需先发布新的程序版本。
2. 用 `docs/mizuki.toml`、`docs/content/` 和 `docs/theme/` 构建静态网页。
3. 使用 Pages 返回的实际站点地址覆盖 `base_url`，处理仓库前缀与自定义域名。
4. 上传 `build/docs/`，由独立的部署任务发布到 `github-pages` 环境。

在仓库 **Settings → Pages → Build and deployment** 中选择 **GitHub Actions**。工作流在推送到 `main` 或手动运行 **Deploy documentation to Pages** 时执行；首次上线前需先把配置、文档、主题和工作流一并提交到远端。私有仓库还需要确认当前 GitHub 套餐支持 Pages；工作流不能绕过套餐限制。

本地可以运行同样的静态构建：

```sh
./build/bin/mizuki build --config docs/mizuki.toml --base-url https://yang-junjie.github.io/Mizuki/
```

这里的地址是本仓库默认的项目站点地址；正式工作流读取 Pages 配置，不依赖这条示例中的硬编码地址。仅在本地检查项目路径时，可以使用 `--base-url /Mizuki/`。

## 为自己的博客配置 GitHub Pages

仓库提供 [工作流示例](https://github.com/Yang-Junjie/Mizuki/blob/main/examples/github-pages.yml)，适用于包含 Mizuki 源码、`content/`、`themes/` 和 `mizuki.toml` 的站点仓库：

1. 将示例复制为 `.github/workflows/pages.yml`。示例监听 `main` 分支，其他默认分支需要修改 `branches`。
2. 在仓库 **Settings → Pages → Build and deployment** 中，将 **Source** 设为 **GitHub Actions**。
3. 提交工作流和站点内容，或手动运行 **Deploy Mizuki to Pages**。

工作流先编译 Mizuki，再通过 `actions/configure-pages` 取得实际 `base_url`，以 `--base-url` 传给构建命令，最后上传并发布 `dist/`。因此项目站点的仓库前缀、用户站点的根路径和已经配置的自定义域名都不需要在工作流里写死。生成的站点文件不包含本地预览刷新脚本。

示例声明 `contents: read`、`pages: write` 和 `id-token: write`，并使用 `github-pages` 部署环境。博客示例仍保留在 `examples/` 中，复制到工作流目录并满足 Pages 设置后即可部署；本仓库自己的工作流则使用 `docs/mizuki.toml`。

如果内容仓库不包含 Mizuki 源码，需要先在自己的工作流中安装或编译 Mizuki，再使用相同的构建命令和 Pages 上传、部署步骤。

工作流结构参考 GitHub 官方的 [自定义 Pages 工作流说明](https://docs.github.com/en/pages/getting-started-with-github-pages/using-custom-workflows-with-github-pages)、[静态站点示例](https://github.com/actions/starter-workflows/blob/main/pages/static.yml) 和 [`configure-pages` 输出定义](https://github.com/actions/configure-pages/blob/v6/action.yml)。

启用首页分页或文章归档时，整个输出目录应一并发布，包括 `page/2/index.html` 等后续分页与 `archives/index.html`。它们的 canonical 和 sitemap 地址使用目录形式（如 `/blog/page/2/`、`/blog/archives/`），托管服务需要支持目录下的 `index.html`。Mizuki 本地预览与 GitHub Pages 都按此结构提供页面。


## 排查发布问题

| 现象 | 检查位置 |
| --- | --- |
| 提示 `Your current plan does not support GitHub Pages for this repository` | 确认仓库可见性与 GitHub 套餐；保留私有仓库时需使用支持 Pages 的套餐 |
| `configure-pages` 提示找不到站点 | 仓库 Settings → Pages，确认 Source 为 GitHub Actions |
| 首页能打开，但资源或深层页面 404 | 确认构建的 `base_url` 与实际发布前缀一致，上传整个输出目录 |
| 链接中重复出现仓库名 | 主题直接使用上下文 URL 字段，避免再拼接 `site.base_path` |
| Pages 显示旧内容 | 检查远端是否包含修改，以及最新部署任务的结果和环境地址 |
| `build` 失败且线上仍是旧版本 | 先修复文档、模板或路径错误；只有完整成功的产物才应部署 |

GitHub Pages 不提供 Mizuki 本地预览的自动重建能力。发布后的更新来自新的工作流构建与部署；本地编辑仍使用 `mizuki serve`。

文档配置与命令入口见[快速开始](getting-started.md)，主题如何引用资源见[主题开发](themes.md)。
