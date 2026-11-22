---
title: 主题开发
description: 从第一个 Inja 布局到组件、配置、模板上下文与自定义页面。
---

Mizuki 使用 Inja 3.5.0 渲染主题，站点配置与主题配置都使用 TOML。主题决定页面结构，构建器提供结构化数据，不预先生成文章列表、导航或分页 HTML。

## 从默认主题开始

先按[快速开始](getting-started.md)创建站点。`init` 已将默认主题完整写入 `themes/default/`，可以直接编辑，也可以复制为自己的主题，并在 `mizuki.toml` 中修改 `theme`。

1. 编辑 `layouts/base.html` 中的共享页头和元信息。
2. 编辑 `layouts/home.html`，用 `posts`、`navigation` 和 `pagination` 决定首页结构。
3. 编辑 `layouts/post.html` 与 `layouts/page.html`，决定正文、标题、日期和目录如何排版。
4. 在 `assets/` 中编写 CSS 或脚本，模板通过 `{{ asset_base }}` 引用。
5. 运行 `mizuki serve`，同时检查首页、独立页面与深层文章；启用分页和归档时也检查相应页面。

下面的配置与模板片段可以组合使用。模板没有固定的 CSS 类名要求，也不要求保留默认主题的 HTML。

## 文件组织

```text
my-theme/
  theme.toml
  layouts/
    base.html
    home.html
    post.html
    page.html
    archives.html
    special/project.html
    partials/navigation.html
  assets/
    css/style.css
```

在站点 `mizuki.toml` 中设置 `theme = "my-theme"`。`assets/` 原样复制到输出的 `assets/`；模板目录、`theme.toml` 不发布。主题配置文件可省略，所有模板始终使用 Inja。

| 默认入口 | 使用时机 |
| --- | --- |
| `home.html` | 首页与后续分页，始终使用 |
| `post.html` | 没有指定 `layout` 的已发布文章 |
| `page.html` | 没有指定 `layout` 的已发布独立页面 |
| `archives.html` | 启用年份归档时 |

只读取实际使用的模板。模板不要求出现任何固定变量；作者自行决定布局与展示字段。

## 表达式、循环与条件

使用 `{{ expression }}` 输出值，`{% statement %}` 控制渲染，`{# comment #}` 写模板注释。例如 `home.html`：

```html
<h1>{{ site.title }}</h1>
{% if length(posts) == 0 %}<p>暂无文章。</p>{% endif %}
<section class="cards">
{% for post in posts %}
  <article>
    <h2><a href="{{ post.url }}">{{ post.title }}</a></h2>
    {% if post.description != "" %}<p>{{ post.description }}</p>{% endif %}
    <time>{{ post.date }}</time>
  </article>
{% endfor %}
</section>
{% if pagination.next_url != "" %}
<a rel="next" href="{{ pagination.next_url }}">下一页</a>
{% endif %}
```

字符串表达式默认进行 HTML 转义，属性值也应始终加引号。Markdown 正文已转换为 HTML，需要显式输出：

```html
<h1>{{ page.title }}</h1>
<main>{{ html(page.content) }}</main>
```

`html()` 仅接收字符串，直接写入当前输出流，应作为独立输出表达式使用。仅将确定需要作为 HTML 输出的值交给它。`{{ page.content }}` 会显示经过转义的 HTML 文本。数据中的模板语法不会再次解析。集合通常通过循环访问其字段。

访问不存在的变量会导致构建失败。读取可选参数时，可用 `default(site.extra.subtitle, "")` 提供回退值；主题常用参数建议直接在 `theme.toml` 声明默认值。

## 组件与继承

`include` 与 `extends` 的路径统一相对主题的 `layouts/` 根目录，与当前模板所在子目录无关。例如 `partials/navigation.html`：

```html
<nav aria-label="主导航">
{% for item in navigation %}
  <a href="{{ item.url }}">{{ item.title }}</a>
{% endfor %}
</nav>
```

`base.html` 定义共享结构与扩展位置：

```html
<!DOCTYPE html>
<html lang="{{ site.language }}">
<head>
  <meta charset="UTF-8">
  <title>{{ page.title }}</title>
  <link rel="stylesheet" href="{{ asset_base }}css/style.css">
  {% block head %}{% endblock %}
</head>
<body>
{% include "partials/navigation.html" %}
{% block content %}{% endblock %}
</body>
</html>
```

`post.html` 填入正文：

```html
{% extends "base.html" %}
{% block content %}
<article>
  <h1>{{ page.title }}</h1>
  <time>{{ page.date }}</time>
  {{ html(page.content) }}
</article>
{% endblock %}
```

子模板可以在重写的 block 内调用 `{{ super() }}` 输出父 block。当前 Inja 版本要求子模板定义需要输出的 block；共享的默认内容应直接放在基础模板中，或在子 block 内显式调用 `super()`。默认主题因此把页面标题直接写在基础模板中，扩展位置使用空 block。

不允许绝对路径、`..` 路径分量、Windows 盘符或反斜杠，以及通过符号链接越出 `layouts/`。模板包含或继承循环会报错，最大加载深度为 64。模板加载、解析、渲染失败都会保留之前成功发布的输出。

## 模板数据

所有入口均获得下列字段；不适用的集合为空数组，未设置的文本通常为空字符串。

| 字段 | 内容 |
| --- | --- |
| `site.title`、`site.description`、`site.author`、`site.language` | 站点元数据 |
| `site.base_url` | 规范化后的配置地址，包含结尾 `/`；未配置域名时为部署路径 |
| `site.base_path` | 编码后的部署路径，如 `/blog/` |
| `site.extra` | 主题默认值与站点覆盖值合并后的参数对象 |
| `page.kind` | `home`、`post`、`page` 或 `archives` |
| `page.title`、`page.date`、`page.description` | 当前页元数据；列表页日期为空，描述使用站点描述；正文页描述为空时回退到站点描述 |
| `page.content` | Markdown 转换并改写链接后的 HTML；列表页为空 |
| `page.url` | 相对当前页的自身地址；列表页为目录形式 |
| `page.canonical_url` | 配置完整域名时的规范地址，否则为空；列表页为目录形式 |
| `page.headings` | 全部正文 h1–h6 标题，条目包含 `level`、`id`、`text`、`url` |
| `asset_base` | 相对当前页的主题资源目录，包含结尾 `/` |
| `feed_url` | 完整 RSS 地址；未配置域名时为空 |
| `navigation` | 导航数组，每项为 `title`、`url` |
| `posts` | 当前首页分页的文章摘要，按发布日期排序 |
| `archives` | 年份分组数组，每项为 `year`、`posts`，仅归档页有数据 |
| `toc` | 正文二、三级标题树，每项为 `level`、`id`、`text`、`url`、`children` |
| `pagination.current_page`、`pagination.total_pages` | 从 1 开始的页码与总页数；非列表页均为 1 |
| `pagination.previous_url`、`pagination.next_url` | 上一页与下一页地址，不存在时为空 |

`posts` 与归档分组中的文章摘要均包含 `title`、`date`、`description`、`url`，其中描述保留文章自身的值，不回退到站点描述。文章摘要不包含全文。`posts` 只在首页及其分页提供数据，完整归档通过 `archives` 获取。

本地文章、导航、分页和资源地址均已适配当前输出路径及中文、空格编码。直接使用相应 URL 字段；不要再拼接 `site.base_path`。`site.extra` 中作者自定义的字符串与 URL 不自动改写。

`page.headings` 按正文顺序保留全部层级；`toc` 只保留 h2/h3，三级标题放在对应二级标题的 `children` 中，孤立三级标题置于顶层，一级标题结束之前的归属范围。标题的 `text` 是纯文本，`url` 是已经编码的本页锚点。默认目录组件位于 `partials/toc.html`，可以修改标记、替换为侧栏，或完全不使用。

## 主题参数

主题的 `theme.toml` 定义默认值：

```toml
name = "My Theme"
engine = "inja"

[extra]
show_toc = true
footer_text = ""
links = ["GitHub"]

[extra.colors]
accent = "blue"
background = "white"
```

站点的 `mizuki.toml` 覆盖参数：

```toml
[extra]
show_toc = false
footer_text = "我的博客"
links = ["GitHub", "RSS"]

[extra.colors]
accent = "red"
```

模板读取 `site.extra.colors.accent` 得到 `red`，读取 `site.extra.colors.background` 仍得到 `white`。表递归合并，数组整体替换，其他值直接替换；布尔值、整数和浮点数保持类型。支持字符串、布尔值、有限数值、数组与表，日期和时间请加引号写成字符串。主题作者应说明参数期望的类型。

`theme.toml` 只接受 `name`、`engine`、`extra`，`engine` 如填写必须为 `inja`。站点根级配置仍检查未知键，主题自由参数全部放在 `[extra]` 下。

默认主题提供 `show_toc = true` 与 `footer_text = ""`。通过站点 `[extra]` 即可隐藏目录或设置页脚文字。`serve` 与 `watch` 会监听主题配置、组件、布局和站点配置，修改后自动重建；失败时继续提供上次成功版本，修复后恢复。

## 为文档选择布局

```md
---
title: 我的项目
date: 2026-11-22
layout: special/project.html
---

项目介绍。
```

`layout` 可用于文章与独立页面，相对 `layouts/`，必须包含完整文件名。省略时分别使用 `post.html` 与 `page.html`。不存在、为空、重复或越界的路径会导致构建失败，并报告源文档。选择布局不改变文章分类、输出地址、排序或草稿规则。

## 从旧主题迁移

这是一次破坏性变更。旧的单花括号占位符不再解析，没有兼容开关。站点配置与 Markdown 正文继续使用原有格式；主题需要按以下规则改写。

| 原写法 | Inja 写法 |
| --- | --- |
| `{title}`、`{date}`、`{description}` | `{{ page.title }}`、`{{ page.date }}`、`{{ page.description }}` |
| `{markdown}` | `{{ html(page.content) }}` |
| `{asset_base}` | `{{ asset_base }}` |
| `{site_title}` 等站点元数据 | `{{ site.title }}`、`{{ site.description }}`、`{{ site.author }}`、`{{ site.language }}` |
| `{base_url}`、`{base_path}` | `{{ site.base_url }}`、`{{ site.base_path }}` |
| `{canonical_url}`、`{feed_url}` | `{{ page.canonical_url }}`、`{{ feed_url }}` |
| `{posts}`、`{navigation}`、`{archives}`、`{toc}` | 循环对应的数组，自行编写 HTML，或复用默认主题组件 |
| `{pagination}`、`{page_number}`、`{page_count}` | 用 `pagination` 对象构建导航与页码 |
| `{publication_head}`、`{feed_link}` | 检查规范地址与订阅地址非空后自行输出 `<link>` 与 `<a>` |

可以先复制默认主题的 `base.html` 与 `partials/` 作为起点，再逐步调整页面结构与样式。

## 用文档站作为完整示例

本站主题位于仓库的 [`docs/theme/`](https://github.com/Yang-Junjie/Mizuki/tree/main/docs/theme)，展示了同一个框架怎样呈现博客以外的页面：

- `base.html` 放共享页头、资源引用和页脚。
- `home.html` 将导航组织为文档入口，而不是展示文章列表。
- `page.html` 把正文与 `toc` 放入文档布局。
- `partials/navigation.html` 用当前页 URL 标记选中项。
- `partials/toc.html` 自行循环标题树，不依赖构建器生成目录 HTML。
- CSS 处理移动端、深色模式、键盘焦点与打印，不需要额外文档服务。

在仓库根目录运行 `./build/bin/mizuki serve --config docs/mizuki.toml` 即可边改边看。首页展示什么、文章是否必须以博客方式呈现，都由主题决定。

## 发布主题前检查

除普通预览外，再用部署子路径验证一次：

```sh
mizuki serve --base-url /blog/
mizuki build --base-url https://example.com/blog/
```

检查首页、嵌套页面、主题资源、站内链接与章节跳转。站点有分页或归档时，检查它们的布局和相对地址。改变布尔参数、嵌套表和数组，确认默认值与覆盖行为符合主题说明。

故意写错一个模板，确认构建报出具体路径，旧网页仍在；修复后确认预览恢复。正文与标题中加入引号、尖括号和 `{{ example }}`，确认普通文本正确转义，正文 HTML 正常输出，示例中的模板语法保持字面内容。

主题使用者需要知道所需入口、参数类型、默认值以及自定义布局名称。生成网页的内部数据流见[架构设计](architecture.md)，最终发布方式见[部署指南](deployment.md)。
