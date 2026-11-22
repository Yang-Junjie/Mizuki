<div align="center">

<img src="./docs/theme/assets/images/logo.webp" width="96" height="96" alt="Mizuki 标志" />

# Mizuki

**用 Markdown 写作，用主题表达自己。**

一个用 C++23 编写的轻量静态博客框架，提供简单、自由、职责清晰的主题开发方式。

[文档站](https://blog.cglab.top/Mizuki/) · [下载程序](https://github.com/Yang-Junjie/Mizuki/releases) · [问题反馈](https://github.com/Yang-Junjie/Mizuki/issues)

[![构建与测试](https://github.com/Yang-Junjie/Mizuki/actions/workflows/ci.yml/badge.svg)](https://github.com/Yang-Junjie/Mizuki/actions/workflows/ci.yml)
![C++23](https://img.shields.io/badge/C%2B%2B-23-f4a7bb)
[![许可证：GPLv3](https://img.shields.io/badge/许可证-GPLv3-a8b8df)](./LICENSE)

</div>

## 从这里开始

[在线文档](https://blog.cglab.top/Mizuki/)是安装、写作、配置、主题开发与部署的主要入口，也由 Mizuki 自身构建。仓库中的 [Markdown 正文](./docs/content/pages/)与文档站共用同一份内容，可直接阅读或在本地预览。

Mizuki 负责把内容、配置和模板生成静态网页；主题负责页面结构、字体、配色和交互。日常建站使用一个程序，不需要安装编译器或前端构建工具；主题开发使用 HTML、Inja、CSS 和可选 JavaScript，不需要修改 C++。

## 可以做什么

| 能力 | 说明 |
| --- | --- |
| 写作与组织 | Markdown 文章、独立页面、草稿、标签、分类、分页和年份归档 |
| 主题自定义 | Inja 继承与组件、全站内容集合、TOML 自定义数据、按页面选择布局 |
| 技术内容 | 标题锚点、章节目录、脚注和提示块；默认主题提供可配置的代码高亮、复制与公式排版 |
| 本地预览 | 监听内容、主题和配置，构建成功后刷新浏览器，失败时保留上次成功输出 |
| 静态发布 | 部署子路径、RSS、sitemap、canonical，生成文件可交给 GitHub Pages 等静态托管 |
| 输出检查 | 构建前检查路径冲突与 Markdown 链接，完整构建成功后替换输出目录 |

## 几步搭建博客

从 [Releases](https://github.com/Yang-Junjie/Mizuki/releases) 下载 Linux 或 Windows x86_64 发布包，解压并将 `bin/` 加入 `PATH`。平台要求、校验方法和源码编译步骤见[快速开始](./docs/content/pages/getting-started.md)。

```sh
mizuki init my-blog
cd my-blog
mizuki new posts/hello.md --title "第一篇文章"
mizuki serve
```

打开 <http://127.0.0.1:3939/>，编辑 `content/` 下的文章或 `themes/default/` 下的主题即可预览。运行 `mizuki build` 生成可部署的 `dist/`。

## 按需阅读

| 我想…… | 阅读 |
| --- | --- |
| 安装程序，搭建第一个站点 | [快速开始](./docs/content/pages/getting-started.md) |
| 设置站点、导航、分页和归档 | [站点配置](./docs/content/pages/configuration.md) |
| 查询命令、参数与预览行为 | [命令行](./docs/content/pages/cli.md) |
| 了解文章字段、站内链接和章节锚点 | [内容与链接](./docs/content/pages/content.md) |
| 编写代码、公式、脚注，使用标签和分类 | [技术写作](./docs/content/pages/writing.md) |
| 制作主题，更换字体、样式和页面布局 | [主题开发](./docs/content/pages/themes.md) · [最小主题示例](./examples/minimal-theme/) |
| 发布到静态托管，配置 RSS 与 sitemap | [部署指南](./docs/content/pages/deployment.md) |
| 理解源码、参与开发或维护版本 | [架构设计](./docs/content/pages/architecture.md) · [参与贡献](./docs/content/pages/contributing.md) · [版本发布](./docs/content/pages/releasing.md) |
| 修改或预览文档站 | [文档维护](./docs/content/pages/documentation.md) |

## 参与项目

欢迎提交问题、修正文档和改进代码。提交前请阅读[贡献入口](./CONTRIBUTING.md)；版本变化见[更新记录](./CHANGELOG.md)。

Mizuki 使用 [GNU GPL v3](./LICENSE)。随附的第三方依赖、字体与图片遵循各自许可证或授权说明；文档站资源来源见[主题资源说明](./docs/theme/ASSETS.md)。
