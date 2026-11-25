+++
title = "主题开发"
description = "从第一个 Inja 布局到组件、配置、模板上下文与自定义页面。"
+++

Mizuki 使用 Inja 3.5.0 渲染主题，站点配置与主题配置都使用 TOML。主题决定页面结构，构建器提供结构化数据，不预先生成文章列表、导航或分页 HTML。

## 从默认主题开始

先按[快速开始](getting-started.md)创建站点。`init` 已将默认主题完整写入 `themes/default/`，可以直接编辑，也可以复制为自己的主题，并在 `mizuki.toml` 中修改 `theme`。

1. 编辑 `layouts/base.html` 中的共享页头和元信息。
2. 编辑 `layouts/home.html`，用 `posts`、`navigation` 和 `pagination` 决定首页结构。
3. 编辑 `layouts/post.html` 与 `layouts/page.html`，决定正文、标题、日期和目录如何排版。
4. 在 `assets/` 中编写 CSS 或脚本，模板通过 `{{ asset_base }}` 引用。
5. 运行 `mizuki serve`，同时检查首页、独立页面与深层文章；启用分页和归档时也检查相应页面。

下面的配置与模板片段可以组合使用。模板没有固定的 CSS 类名要求，也不要求保留默认主题的 HTML。

## 从最小主题开始

如果想从少量代码理解接口，可直接使用仓库的 [`examples/minimal-theme/`](https://github.com/Yang-Junjie/Mizuki/tree/main/examples/minimal-theme)。它包含基础布局、六个入口和一份 CSS，不需要 C++ 修改、前端构建工具或脚本。

在仓库根目录运行：

```sh
./build/bin/mizuki build --config docs/mizuki.toml \
  --theme examples/minimal-theme --output build/minimal-theme --base-url /Mizuki/
```

文档全部是独立页面，因此首页的文章列表为空，页头会自动列出所有文档。用于自己的博客时，将该目录复制到站点的 `themes/minimal/`，设置 `theme = "themes/minimal"`，再运行 `mizuki serve`。最小主题同时支持文章、独立页面、分页、归档、标签和分类。

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
    taxonomy.html
    term.html
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
| `taxonomy.html` | 有已发布标签或分类时，用于各自的总览 |
| `term.html` | 有已发布标签或分类时，用于单个条目的文章列表与分页 |

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

主题数据分为三个范围：

| 范围 | 使用的字段 | 职责 |
| --- | --- | --- |
| 全站 | `site`，包括 `site.posts`、`site.pages`、`site.tags`、`site.categories` | 站点设置与全部已发布内容摘要，所有入口都可读取 |
| 当前页 | `page`、`toc` | 正在渲染的页面，包括它自己的正文与标题 |
| 当前列表 | `posts`、`archives`、`pagination`、`taxonomy`、`terms`、`term` | 首页或条目分页的切片、归档年份分组、标签与分类数据 |

全站集合不受当前页类型和分页影响。读取集合只提供数据，额外输出页面仍由构建计划决定。所有入口均获得下列字段；不适用的集合为空数组，未设置的文本通常为空字符串。

| 字段 | 内容 |
| --- | --- |
| `site.title`、`site.description`、`site.author`、`site.language` | 站点元数据 |
| `site.base_url` | 规范化后的配置地址，包含结尾 `/`；未配置域名时为部署路径 |
| `site.base_path` | 编码后的部署路径，如 `/blog/` |
| `site.extra` | 主题默认值与站点覆盖值合并后的参数对象 |
| `site.posts` | 全站已发布文章摘要，日期降序，同日按输出路径升序 |
| `site.pages` | 全站已发布独立页面摘要，按输出路径升序 |
| `site.tags`、`site.categories` | 全站已发布标签与分类，条目为 `name`、`url`、`count`，按名称 UTF-8 字节排序 |
| `page.kind` | `home`、`post`、`page`、`archives`、`taxonomy` 或 `term` |
| `page.title`、`page.date`、`page.description` | 当前页元数据；列表页日期为空，描述使用站点描述；正文页描述为空时回退到站点描述 |
| `page.extra` | 当前文档的自定义参数对象；未声明时与集合页均为空对象 |
| `page.tags`、`page.categories` | 当前文章的条目链接，每项为 `name`、`url`；其他页面为空数组 |
| `page.content` | Markdown 转换并改写链接后的 HTML；列表页为空 |
| `page.url` | 相对当前页的自身地址；列表页为目录形式 |
| `page.canonical_url` | 配置完整域名时的规范地址，否则为空；列表页为目录形式 |
| `page.headings` | 全部正文 h1–h6 标题，条目包含 `level`、`id`、`text`、`url` |
| `page.word_count` | 文章页的正文字数 |
| `page.features` | `code`、`math`、`footnotes`、`admonitions` 布尔值，表示当前正文含有相应标记；集合页均为 false |
| `page.code_languages` | 代码块语言名称，按首次出现顺序去重，保留源语言与别名；集合页为空 |
| `asset_base` | 相对当前页的主题资源目录，包含结尾 `/` |
| `feed_url` | 完整 RSS 地址；未配置域名时为空 |
| `navigation` | 导航数组，每项为 `title`、`url` |
| `posts` | 当前首页或标签／分类条目分页的文章摘要，按发布日期排序 |
| `taxonomy`、`terms`、`term` | 当前聚合页数据，详见下文的标签与分类接口 |
| `archives` | 年份分组数组，每项为 `year`、`posts`，仅归档页有数据 |
| `toc` | 正文二、三级标题树，每项为 `level`、`id`、`text`、`url`、`children` |
| `pagination.current_page`、`pagination.total_pages` | 从 1 开始的页码与总页数；非列表页均为 1 |
| `pagination.previous_url`、`pagination.next_url` | 上一页与下一页地址，不存在时为空 |

`site.posts`、`site.pages`、`posts` 与归档分组中的摘要共用以下字段：

| 摘要字段 | 语义 |
| --- | --- |
| `title` | 文稿自己的标题 |
| `url` | 已编码、相对当前输出页的链接，可直接用于 `href` |
| `date` | 文稿自己的日期；独立页面允许没有日期，此时为空字符串 |
| `description` | 文稿自己的描述；没有填写时为空，不回退到站点描述 |
| `extra` | 文稿自己的自定义参数；保留布尔、数值、数组与嵌套表，未设置时为空对象 |
| `tags`、`categories` | 文稿的条目链接，条目为 `name`、`url`，按文稿中首次出现的顺序排列；独立页面为空数组 |

摘要不含正文、标题列表或阅读特征；当前正文通过 `page.content` 获取。`extra` 不与 `site.extra` 合并。当前文章或独立页面也包含在它自己的全站集合中。

`site.pages` 只收集 `content/pages/` 下已发布的 Markdown；文章、首页分页、归档、标签／分类页和复制的 HTML 资源均不在其中。草稿不出现在任何发布集合。排序比较的是未编码输出路径的 UTF-8 字节，独立页面不按日期或标题排序；需要手动排序的导航可以继续使用配置中的 `navigation`。

根级 `posts` 只在首页及其分页、标签／分类条目页及其分页提供当前切片；在详情页、独立页面、归档和总览页为空。`archives` 只在归档页提供完整年份分组，在其他入口为空。其他入口读取最近文章时应使用 `site.posts`。

首页和首页分页中的 `posts` 另外提供整数 `post.word_count`，与对应文章页的 `page.word_count` 一致。统计包含 Markdown 正文、正文标题和可见的链接标签。每个汉字和每个拉丁字母各计一字，数字、标点、代码、已解析的数学公式、图片替代文本和链接地址略过。

全站摘要、当前列表、导航、分页和资源地址均已适配当前输出路径及中文、空格编码，包括深层正文、后续分页与归档页。直接使用相应 URL 字段，不再拼接 `site.base_path`，也不再传给 `url()`。`site.extra`、`page.extra` 和摘要 `extra` 中的自定义字符串与 URL 不自动改写。模板中的站内资源可通过 `url()` 生成地址，详见下文。

`page.headings` 按正文顺序保留全部层级；`toc` 只保留 h2/h3，三级标题放在对应二级标题的 `children` 中，孤立三级标题置于顶层，一级标题结束之前的归属范围。标题的 `text` 是纯文本，`url` 是已经编码的本页锚点。默认目录组件位于 `partials/toc.html`，可以修改标记、替换为侧栏，或完全不使用。

## 在任意页面组合全站内容

下面的组件可以包含在基础布局中，因此首页、文章、独立页面与归档页都能展示最近五篇文章：

```html
{% if length(site.posts) > 0 %}
<aside aria-label="最近文章">
  <h2>最近文章</h2>
  <ul>
  {% for entry in site.posts %}{% if loop.index < 5 %}
    <li><a href="{{ entry.url }}">{{ entry.title }}</a></li>
  {% endif %}{% endfor %}
  </ul>
</aside>
{% endif %}
```

`loop.index` 从 0 开始，`loop.index1` 从 1 开始。默认主题在 `partials/recent-posts.html` 中实现了这个用法，并将开关和数量放在主题参数里。

独立页面也可以自动形成导航。循环变量使用 `entry`，保留 `page` 表示当前页：

```html
<nav aria-label="独立页面">
{% for entry in site.pages %}
  <a href="{{ entry.url }}"{% if entry.url == page.url %} aria-current="page"{% endif %}>{{ entry.title }}</a>
{% endfor %}
</nav>
```

自定义集合展示条件也只需要模板。例如文稿的 `[extra]` 中设置 `featured = true`，主题就能从 `site.posts` 或 `site.pages` 挑出推荐内容：

```html
{% for entry in site.posts %}
  {% if default(entry.extra.featured, false) %}
  <article><h2><a href="{{ entry.url }}">{{ entry.title }}</a></h2>
    <p>{{ entry.description }}</p>
  </article>
  {% endif %}
{% endfor %}
```

这些列表使用已有输出地址，在当前页面中展示内容。自定义样式、数量和筛选条件属于主题；模板不会为筛选结果自动创建新文件。

## 标签与分类页面

构建器根据文章根级 `tags`、`categories` 自动创建页面；主题负责其 HTML、样式与交互，不需修改 C++。文稿格式、名称限制和生成时机见[技术写作](writing.md#标签与分类)。没有已发布条目时，不生成该类型的总览或列表，也不读取对应布局。

两个布局共用于标签与分类，可按 `taxonomy.name` 区分：

| 字段 | `taxonomy.html` 总览 | `term.html` 文章列表 |
| --- | --- | --- |
| `page.kind` | `taxonomy` | `term` |
| `taxonomy` | `{name, title, url}` | `{name, title, url}` |
| `terms` | 当前类型全部条目的 `{name, url, count}` 数组 | 空数组 |
| `term` | `{name: "", url: "", count: 0}` | 当前条目的 `{name, url, count}` |
| `posts` | 空数组 | 当前条目的文章分页切片 |
| `pagination` | 默认一页 | 与首页相同的页码和前后页链接 |

`taxonomy.name` 为 `tags` 或 `categories`，`title` 分别为“标签”或“分类”，`url` 指向相应总览。`term.count` 是当前条目的全部已发布文章数，`term.url` 始终指向该条目第一页。所有 URL 均相对当前输出页，包含编码，可直接用于 `href`。`site.tags`、`site.categories` 始终是全站数据，不受当前条目或分页切片限制；其 `count` 同样是已发布文章数。

其他页面上的 `terms` 为空数组，`taxonomy` 的三个文本字段为空字符串，`term` 的名称与 URL 为空、数量为 `0`。当前条目页的 `page.tags`、`page.categories` 也为空；它们只描述当前文章。

总览布局可以这样写：

```html
{% extends "base.html" %}
{% block content %}
<h1>{{ taxonomy.title }}</h1>
<ul>
{% for entry in terms %}
  <li><a href="{{ entry.url }}">{{ entry.name }}</a>（{{ entry.count }}）</li>
{% endfor %}
</ul>
{% endblock %}
```

条目布局可以复用已有文章列表和分页组件，也可以独立编写：

```html
{% extends "base.html" %}
{% block content %}
<a href="{{ taxonomy.url }}">全部{{ taxonomy.title }}</a>
<h1>{{ term.name }}</h1>
<p>{{ term.count }} 篇文章</p>
{% for post in posts %}
  <article><a href="{{ post.url }}">{{ post.title }}</a></article>
{% endfor %}
{% if pagination.next_url != "" %}
<a rel="next" href="{{ pagination.next_url }}">下一页</a>
{% endif %}
{% endblock %}
```

文章布局可循环 `page.tags`、`page.categories`，列表卡片可循环 `post.tags`、`post.categories`。标签链接可添加 `rel="tag"`。这些名称自动 HTML 转义，URL 不应再次交给 `url()`。全站导航可以用 `url("tags/")` 和 `url("categories/")`，但要先确认相应 `site` 数组非空，避免指向未生成的总览。

条目列表与首页共用 `paginate_by` 和文章排序；新增条目输出也参与冲突检查、旧产物清理与 sitemap。主题可以显示标签云、分类导航或文章徽标；本轮只提供标签和扁平分类，未提供自定义 slug、层级分类或其他 taxonomy 类型。

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

默认主题提供 `show_toc = true`、`show_recent_posts = true`、`recent_posts_limit = 5` 与 `footer_text = ""`。通过站点 `[extra]` 可以隐藏目录、关闭最近文章或设置页脚文字；`recent_posts_limit` 使用非负整数，设为 `0` 也会隐藏最近文章。文档 `[extra].show_toc` 可单独覆盖目录开关。`serve` 与 `watch` 会监听主题配置、组件、布局和站点配置，修改后自动重建；失败时继续提供上次成功版本，修复后恢复。

## 页面自定义数据

由 `+++` 包裹的 front matter 使用 TOML，与站点配置共用 toml++ 解析器。内置字段之外的数据全部放在 `[extra]` 中：

```toml
+++
title = "我的第一篇文章"
date = "2026-11-22"
description = "页面与文章列表使用的摘要。"
draft = false

[extra]
cover = "images/cover.webp"
cover_alt = "山间的晨光"
show_toc = false
featured = true
reading_minutes = 5
labels = ["C++", "图形学"]

[extra.author]
name = "Artichoke"
bio = "记录图形学与日常开发"
+++
```

`page.extra.cover`、`page.extra.author.name` 可在文章或独立页面布局中直接读取。首页及其分页通过 `post.extra` 读取同一篇文章的数据，归档的 `group.posts` 也提供相同的 `extra` 对象。自定义数据不改变输出路径、文章排序或草稿规则。

```html
{% if default(page.extra.featured, false) %}<p>精选文章</p>{% endif %}
<p>{{ default(page.extra.author.name, site.author) }}</p>
{% for label in default(page.extra.labels, []) %}<span>{{ label }}</span>{% endfor %}
```

支持字符串、布尔值、有限数值、数组、表与表数组，类型保留到模板；日期与时间值应加引号作为字符串。省略 `[extra]` 时得到空对象。`page.extra` 保留页面自己声明的数据，不与 `site.extra` 隐式合并；模板可以逐项定义回退规则：

```html
{% if default(page.extra.show_toc, site.extra.show_toc) and length(toc) > 0 %}
  {% include "partials/toc.html" %}
{% endif %}
```

默认主题的目录组件采用上述优先级：文档 `show_toc` 优先于站点配置，站点配置优先于主题默认值；显式的 `false` 会保留。关闭目录仍保留标题 ID、章节链接与 `page.headings`、`toc` 数据。`show_toc` 应为布尔值，封面地址和说明应为字符串；这些是默认主题的约定，自定义主题可自行定义参数。

### 封面与资源地址

默认主题支持 `cover` 与可选的 `cover_alt`。`cover` 可以是相对站点根目录的资源路径，如 `images/cover.webp` 或 `/images/cover.webp`，也可以是完整的外部图片地址；它不相对 Markdown 所在目录解析。图片放入内容目录或主题 `assets/`，例如 `content/images/cover.webp` 对应 `cover = "images/cover.webp"`。

```html
{% if default(page.extra.cover, "") != "" %}
<img src="{{ url(page.extra.cover) }}" alt="{{ default(page.extra.cover_alt, "") }}">
{% endif %}

{% for post in posts %}
<article>
  {% if default(post.extra.cover, "") != "" %}
  <img src="{{ url(post.extra.cover) }}" alt="" loading="lazy">
  {% endif %}
  <h2><a href="{{ post.url }}">{{ post.title }}</a></h2>
</article>
{% endfor %}
```

`url(value)` 从站点根目录解析本地地址，加入配置的部署前缀，并编码中文、空格等路径字符，保留查询参数和片段。例如部署在 `/Mizuki/` 时，`url("images/封面 image.webp")` 输出 `/Mizuki/images/%E5%B0%81%E9%9D%A2%20image.webp`。外部 URL、协议相对 URL、空字符串、仅查询参数或锚点的地址原样保留。本地路径不得越出站点根目录；函数只生成 URL，不检查资源是否存在，也不转换 Markdown 扩展名。

普通模板输出仍进行 HTML 转义，包括 URL 与 `cover_alt`。`url()` 用于作者提供的资源路径；`post.url`、`navigation`、`asset_base` 等已有 URL 字段可以直接使用，无需再次处理。

### 内置字段校验

TOML front matter 的根级只接受 `title`、`date`、`description`、`draft`、`layout`、`tags`、`categories`、`extra`。未知键会提示将自定义数据移入 `[extra]`；重复键、TOML 语法错误及字段类型错误都会中止构建，并保留上次成功输出。语法错误包含源文档路径及行列位置。

- `title` 必须是非空字符串。
- 文章的 `date` 必填，独立页面可省略；提供时必须是有效的 `YYYY-MM-DD` 字符串或 TOML 本地日期，不接受日期时间。
- `description` 如提供必须是字符串，可以为空。
- `layout` 如提供必须是非空字符串，路径仍受主题目录边界约束。
- `draft` 如提供必须是布尔值。
- `tags`、`categories` 如提供必须是字符串数组，仅用于文章，条目名称规则见[技术写作](writing.md#标签与分类)。
- `extra` 如提供必须是表；表内字段由主题定义，仍要求值能转换为上述模板数据类型。

已有 `---` 包裹的简单 `key: value` 文稿继续支持，它不是完整 YAML，不提供嵌套数据；没有自定义数据的文稿无需迁移。`init` 与 `new` 统一生成 TOML 格式。

## 正文能力与资源

框架提供 `page.features` 和 `page.code_languages`，主题自行选择渲染库、样式与交互。例如只在包含公式的页面加载本地脚本：

```html
{% if page.features.math %}
<link rel="stylesheet" href="{{ asset_base }}vendor/katex/katex.min.css">
<script defer src="{{ asset_base }}vendor/katex/katex.min.js"></script>
<script defer src="{{ asset_base }}js/math.js"></script>
{% endif %}
<article data-markdown>{{ html(page.content) }}</article>
```

这些字段从生成的 HTML 收集；原始 HTML 中的实际标记也会被识别，注释、脚本和代码文字中的伪标记不算。`code` 包含未指定语言的代码块；`code_languages` 不表示语言一定受当前高亮库支持。数学节点只由启用的数学语法或作者直接写入的 HTML 产生。

默认主题与文档主题使用以下结构和公开钩子：

| 内容 | 构建生成的 HTML | 主题增强与样式 |
| --- | --- | --- |
| 代码块 | `pre > code.language-cpp`，未标注语言时没有 language 类 | `.hljs-*`、`.code-block`、`.code-toolbar`、`.code-language`、`.code-copy` |
| 行内公式 | `x-equation`，内容为已转义的 TeX | `.math-rendered`、`.math-error`、KaTeX 的 `.katex` |
| 独立公式 | `x-equation[type="display"]` | 区域内横向滚动，HTML 与 MathML |
| 脚注 | `section.footnotes`、`li#fn-N`、`sup` 内引用 | `.footnote-backref`，每次引用均有对应返回链接 |
| 提示块 | `div.admonition-note` 等五种类型、`p.admonition-title` | `note`、`tip`、`important`、`warning`、`caution` 各自配色 |

复用默认 `code.js`、`math.js` 时，在正文容器加 `data-markdown`；默认 `partials/reading.html` 负责按需引用 `reading.css`、本地库和初始化脚本。代码按明确且受支持的语言高亮，不做自动检测，复制保留原始文本。数学脚本只处理 `x-equation`，不会扫描其他正文，也不会处理 `pre`、`code` 中的节点。

默认主题约定 `site.extra.code_highlight`、`code_copy`、`math_render` 为布尔值，默认均为 true。其他主题可定义自己的参数，不受这组名称限制。语法开关使用站点 `[markdown]`，见[技术写作](writing.md)。

`reading.css` 提供以下变量，可直接修改或在后续样式表覆盖：

- 语法颜色：`--syntax-comment`、`--syntax-keyword`、`--syntax-string`、`--syntax-number`、`--syntax-title`、`--syntax-variable`、`--syntax-addition`、`--syntax-deletion`。
- 代码排版：`--code-font-size`、`--code-line-height`、`--code-radius`。
- 公式与脚注：`--math-font-size`、`--footnote-font-size`。
- 提示块：`--admonition-note`、`--admonition-tip`、`--admonition-important`、`--admonition-warning`、`--admonition-caution`。

颜色随系统浅色、深色偏好切换；覆盖颜色时为两种模式分别设值。复制按钮、长代码、宽公式和打印样式也可以通过对应类调整。可完全替换这些 CSS、脚本或渲染库，只需让模板输出的 HTML 与新脚本匹配。

仓库自带 highlight.js 11.12.0（common 加 CMake，BSD-3-Clause）与 KaTeX 0.19.1（MIT）。KaTeX CSS 的字体来源缩减为 WOFF2，所有引用字体随主题提供。版本与许可证位于各主题 `assets/vendor/`，`tools/update_reading_assets.py` 使用固定包地址与 SHA-512 校验更新两份依赖。直接复制主题即可离线使用，不需要 npm 或运行时 CDN。维护主题脚本、样式和阅读组件时，应同步两个随仓库提供的主题；回归测试检查其一致性。

## 为文档选择布局

```md
+++
title = "我的项目"
date = "2026-11-22"
layout = "special/project.html"
+++

项目介绍。
```

`layout` 可用于文章与独立页面，相对 `layouts/`，必须包含完整文件名。省略时分别使用 `post.html` 与 `page.html`。不存在、为空、重复或越界的路径会导致构建失败，并报告源文档。选择布局不改变文稿类型、标签与分类、输出地址、排序或草稿规则。

## 从旧主题迁移

这是一次破坏性变更。旧的单花括号占位符不再解析，没有兼容开关。站点配置与 Markdown 正文保持原有语义，已有 `---` 文稿继续支持；主题需要按以下规则改写。

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
- `home.html` 用 `site.pages` 自动生成文档入口卡片，直接读取文稿的标题和描述。新增已发布页面会自动出现在首页；`navigation` 单独控制侧栏的人工排序。
- `page.html` 把正文与 `toc` 放入文档布局。
- `partials/navigation.html` 用当前页 URL 标记选中项。
- `partials/toc.html` 自行循环标题树，不依赖构建器生成目录 HTML。
- CSS 处理移动端、深色模式、键盘焦点与打印，不需要额外文档服务。

文档卡片默认使用文稿自己的 `description`，可以通过站点 `[extra.guide_descriptions]` 按标题覆盖个别卡片说明，不需要在主题中重复维护每篇文档的元数据。

在仓库根目录运行 `./build/bin/mizuki serve --config docs/mizuki.toml` 即可边改边看。首页展示什么、文章是否必须以博客方式呈现，都由主题决定。

## 发布主题前检查

除普通预览外，再用部署子路径验证一次：

```sh
mizuki serve --base-url /blog/
mizuki build --base-url https://example.com/blog/
```

检查首页、嵌套页面、主题资源、站内链接与章节跳转。站点有分页、归档、标签或分类时，检查它们的布局和相对地址，包括中文与特殊字符名称、条目分页及计数。改变布尔参数、嵌套表和数组，确认默认值与覆盖行为符合主题说明。

故意写错一个模板，确认构建报出具体路径，旧网页仍在；修复后确认预览恢复。正文与标题中加入引号、尖括号和 `{{ example }}`，确认普通文本正确转义，正文 HTML 正常输出，示例中的模板语法保持字面内容。

主题使用者需要知道所需入口、参数类型、默认值以及自定义布局名称。生成网页的内部数据流见[架构设计](architecture.md)，最终发布方式见[部署指南](deployment.md)。
