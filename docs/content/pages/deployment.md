+++
title = "部署指南"
description = "将 Mizuki 生成的静态网页部署到 GitHub Pages，正确处理域名与部署子路径。"
+++

站点发布地址通过 `mizuki.toml` 的 `base_url` 设置，也可以使用 `--base-url` 临时覆盖。构建输出由静态文件组成，不需要在发布服务器上运行 Mizuki。

## 部署地址与子路径

`base_url` 默认为 `/`，支持以 `/` 开始的站点路径，或包含域名的 HTTP(S) 地址。缺少结尾 `/` 时会自动补齐，路径中的中文、空格和百分号按 URL 编码规范化。例如：

```toml
base_url = "https://example.com/blog/"
```

部署到域名根目录时使用 `https://example.com/`；本地只需要模拟子路径时可以使用 `/blog/`。地址不允许用户名、密码、query、fragment、`.` / `..` 路径段、连续 `/`、控制字符或无效百分号编码；端口范围为 1–65535。域名使用 ASCII 或 Punycode，可使用方括号包裹的 IPv6 地址。`--base-url` 在 `build`、`watch`、`serve` 中均可覆盖配置，并在配置热重载后保留优先级。

内容和导航中的根路径始终从**站点根目录**解析，书写时不包含部署前缀。例如部署到 `/blog/` 时：

| 内容 | 生成结果 |
| --- | --- |
| 正文 `[关于](/pages/about.md)` | `href="/blog/pages/about.html"` |
| 正文 `[下载](/download.zip)` | `href="/blog/download.zip"` |
| 正文 `![图片](/images/photo.png)` | `src="/blog/images/photo.png"` |
| 正文 `[关于](../pages/about.md)` | `href="../pages/about.html"` |
| 默认主题样式、导航、首页文章列表 | 保持相对链接 |

query 和 fragment 保留；外部地址、`//` 地址、纯锚点保持原样。`base_url` 中的域名不会强行加入普通站内链接。输出文件仍直接位于 `dist/`，不会额外创建 `dist/blog/`；部署时将输出目录的内容挂载到 `/blog/`。

`serve` 使用相同的部署路径。例如运行 `mizuki serve --base-url /blog/` 后访问 `http://127.0.0.1:3939/blog/`；`/blog` 会重定向到 `/blog/`，前缀之外的地址返回 404。静态文件与自动刷新 SSE 均使用该前缀。修改 `base_url` 后，构建成功才切换预览路径，并通知已打开的页面转到新路径，保留页面路径、query 和 fragment；构建失败时保留旧输出、旧预览路径和刷新版本。


## 先在本地验证

假设站点地址是 `https://example.com/blog/`：

```toml
base_url = "https://example.com/blog/"
```

```sh
mizuki serve
# 打开 http://127.0.0.1:3939/blog/
mizuki build
```

检查首页、深层文章、独立页面、图片和下载链接，然后把 `dist/` 中的内容发布到该地址。不要再将整个输出包入 `blog/` 目录。部署到域名根目录时使用 `https://example.com/` 或 `/`。

配置的域名始终是正式发布域名，本地预览仍监听本机地址；在预览中点击 RSS 入口会访问配置的正式地址。

文章中仍写 `[关于](/pages/about.md)`、`![图片](/images/photo.png)`，无需手动加入 `/blog/`；相对链接也可继续使用。自定义主题使用 `{{ asset_base }}` 引用主题资源，使用 `{{ site.base_path }}` 引用其他站点资源。原样复制的 HTML、CSS 和 `srcset` 请使用适当的相对 URL。

## RSS、sitemap 与规范地址

将 `base_url` 设置为包含域名的完整 HTTP(S) 发布地址即可启用：

```toml
base_url = "https://example.com/blog/"
```

成功构建后会生成 `dist/rss.xml` 和 `dist/sitemap.xml`。输出目录仍不包含 `blog/` 前缀：RSS、sitemap 和默认主题的 canonical 地址统一使用完整发布地址，并对中文、空格和特殊字符进行 URL 编码。

- RSS 2.0 只包含已发布文章，使用标题、发布日期、可选描述和页面完整地址；文章地址也作为稳定的 GUID。独立页面、草稿和原样复制的 HTML 不进入订阅源。
- 发布日期只有日历日期，RSS 将其明确表示为当天 `00:00:00 GMT`，不依赖本机时区或语言环境。订阅源不写入当前构建时间，相同输入的输出保持一致。
- sitemap 包含首页及其分页、归档、标签与分类总览及其全部分页、已发布文章和独立页面。列表使用目录地址，文稿使用实际 `.html` 地址；不将发布日期或文件时间冒充最后修改时间。当前生成单个文件，最多 50,000 个地址、50 MiB，单个地址少于 2,048 字符；超限会报错，不自动拆分。
- 默认主题在首页、文章和独立页面的 `<head>` 中加入 canonical 和 RSS 自动发现链接，在页脚显示 RSS 入口。自定义主题使用 `page.canonical_url` 与 `feed_url` 自行输出标签。

当 `base_url` 只是 `/` 或 `/blog/` 时，正常构建和预览，并打印说明；不生成这两个 XML 文件，也不插入 canonical 和订阅链接。切换配置后，下一次成功构建会移除旧的生成文件。没有完整发布地址时，内容目录里自行提供的同名 XML 仍可原样复制。

启用生成后，`rss.xml`、`sitemap.xml` 是受管理的输出路径，与内容文件冲突时会明确报出生成器和输入来源。生成失败会保留之前的完整站点。

## 本仓库的文档站

Mizuki 仓库已经提供实际使用的 [Pages 工作流](https://github.com/Yang-Junjie/Mizuki/blob/main/.github/workflows/pages.yml)。它部署的就是正在阅读的文档站，流程如下：

1. 从当前提交编译 Mizuki，无需先发布新的程序版本。
2. 用 `docs/mizuki.toml`、`docs/content/` 和 `docs/theme/` 构建静态网页。
3. 使用 Pages 返回的实际站点地址覆盖 `base_url`，处理仓库前缀与自定义域名。
4. 上传 `build/docs/`，由独立的部署任务发布到 `github-pages` 环境。

在仓库 **Settings → Pages → Build and deployment** 中选择 **GitHub Actions**。工作流在推送到 `main` 或手动运行 **Deploy documentation to Pages** 时执行；首次上线前需先把配置、文档、主题和工作流一并提交到远端。私有仓库还需要确认当前 GitHub 套餐支持 Pages；工作流不能绕过套餐限制。

本地可以运行同样的静态构建：

```sh
./build/bin/mizuki build --config docs/mizuki.toml --base-url https://blog.cglab.top/Mizuki/
```

这里使用文档站的发布地址；正式工作流读取 Pages 配置，不依赖这条示例中的硬编码地址。仅在本地检查项目路径时，可以使用 `--base-url /Mizuki/`。

## 为自己的博客配置 GitHub Pages

仓库提供 [工作流示例](https://github.com/Yang-Junjie/Mizuki/blob/main/examples/github-pages.yml)，适用于包含 Mizuki 源码、`content/`、`themes/` 和 `mizuki.toml` 的站点仓库：

1. 将示例复制为 `.github/workflows/pages.yml`。示例监听 `main` 分支，其他默认分支需要修改 `branches`。
2. 在仓库 **Settings → Pages → Build and deployment** 中，将 **Source** 设为 **GitHub Actions**。
3. 提交工作流和站点内容，或手动运行 **Deploy Mizuki to Pages**。

工作流先编译 Mizuki，再通过 `actions/configure-pages` 取得实际 `base_url`，以 `--base-url` 传给构建命令，最后上传并发布 `dist/`。因此项目站点的仓库前缀、用户站点的根路径和已经配置的自定义域名都不需要在工作流里写死。生成的站点文件不包含本地预览刷新脚本。

示例声明 `contents: read`、`pages: write` 和 `id-token: write`，并使用 `github-pages` 部署环境。博客部署的工作流模板位于 `examples/github-pages.yml`，需配合自己的站点配置与内容使用；本仓库自己的工作流使用 `docs/mizuki.toml`。

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
