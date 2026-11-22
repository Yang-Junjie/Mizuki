+++
title = "站点配置"
description = "查阅 mizuki.toml 字段、路径基准、导航、分页、归档与主题参数。"
+++

站点配置使用 TOML。第一次建站可以先阅读[快速开始](getting-started.md)，需要检查某个字段时再回到本页。

## 配置示例与校验

默认读取当前工作目录的 `mizuki.toml`，文件不存在时沿用内置默认值。下面是一个博客站点的配置示例：

```toml
title = "Mizuki"
description = "一个用 C++ 编写的轻量博客框架"
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
```

除上例字段外，还支持 `[markdown]` 和 `[extra]` 两个配置表。`paginate_by` 必须是非负整数，`archives` 必须是布尔值；上例中的其他标量字段均为字符串。字符串字段中，`description` 和 `author` 允许为空，其余不能为空或全是空白。导航项只能包含非空字符串 `title` 和 `url`。省略导航时默认只有首页，`navigation = []` 隐藏导航。未知键、类型错误、重复键或 TOML 语法错误都会报错，并给出配置文件位置。

## 默认值

下面列出未提供配置值时的内置默认值。`mizuki init` 生成的配置显式启用分页、归档和数学语法，因而与部分内置默认值不同。

| 字段 | 默认值 | 用途 |
| --- | --- | --- |
| `title` | `"Mizuki"` | 站点标题 |
| `description` | `"一个用 C++ 编写的轻量博客框架"` | 站点描述 |
| `author` | `""` | 站点作者 |
| `language` | `"zh-CN"` | 页面语言 |
| `base_url` | `"/"` | 发布地址或部署路径 |
| `content` | `"content"` | 内容目录 |
| `theme` | `"themes/default"` | 主题目录 |
| `output` | `"dist"` | 构建输出目录 |
| `paginate_by` | `0` | 每页文章数；`0` 表示不分页 |
| `archives` | `false` | 是否生成年份归档 |
| `navigation` | 只有首页 | 导航列表；`[]` 隐藏导航 |
| `markdown.math` | `false` | 是否解析数学语法 |
| `markdown.footnotes` | `true` | 是否解析脚注 |
| `markdown.admonitions` | `true` | 是否解析提示块 |
| `extra` | 空表 | 站点提供的主题自定义参数 |

## 优先级与路径基准

优先级为 **命令行 > 配置文件 > 内置默认值**，参数先后顺序不影响优先级。

- 默认读取当前工作目录的 `mizuki.toml`，不存在时使用内置默认值。
- 配置中的相对目录，以及已存在配置文件中省略的目录默认值，以配置文件所在目录为基准。
- 命令行相对路径和 `--config` 路径以当前工作目录为基准。
- 显式指定的 `--config` 文件不存在时会报错。

```sh
mizuki build --config sites/blog.toml
mizuki serve --config sites/blog.toml --output preview
mizuki watch --content content --theme themes/default --output dist
mizuki build --base-url https://example.com/blog/
```

目录覆盖参数和配置热重载的行为见[命令行](cli.md)。输出目录的保护规则见[输出管理](cli.md#路径冲突与输出管理)。

## 导航

```toml
[[navigation]]
title = "关于"
url = "/pages/about.md"
```

每项只允许非空字符串 `title` 和 `url`。省略导航时默认只有首页，`navigation = []` 隐藏导航。本地地址从站点根目录解析，可写 `/`、`/pages/about.md` 或 `pages/about.html`；Mizuki 会转换为当前页面可用的相对链接，目标必须是实际发布的页面或静态文件。

外部导航允许 `http://`、`https://` 和 `//` 地址。站内地址无需包含部署前缀；规则见[内容与链接](content.md#站内链接与资源)。

## 首页分页与文章归档

```toml
paginate_by = 10
archives = true
```

`paginate_by` 设置每页文章数，内置默认值 `0` 表示不分页；`archives` 控制年份归档，内置默认值为 `false`。`mizuki init` 生成的站点显式启用这两项；已有配置省略时使用内置默认值。两项相互独立，也可以只启用其中一项。

首页仍位于 `/`，第二页起使用 `/page/2/`、`/page/3/`，对应输出 `page/2/index.html` 等文件。不额外生成 `/page/1/`。空站点也有一张首页；文章数不超过每页数量时不生成额外分页，也不显示分页导航。默认主题在后续分页的浏览器标题中加入页码。

启用归档后生成 `/archives/`，对应输出 `archives/index.html`，按年份降序分组，列出全部已发布文章；同一年内沿用日期降序、同日按输出 URL 排序。草稿、独立页面和复制的 HTML 文件不进入文章列表。没有文章时归档显示空状态。可在导航中加入入口：

```toml
[[navigation]]
title = "归档"
url = "/archives/"
```

分页和归档的文章链接、主题资源和主导航都相对当前页面生成，支持根目录与子路径部署。配置完整 `base_url` 时，每页 canonical 使用对应的目录地址，sitemap 收录首页、后续分页、归档以及内容页面；RSS 始终使用全部已发布文章，不受分页数量影响。

分页输出与归档也参与构建前的路径冲突检查。文章删除、改为草稿、调大分页数量或关闭功能后，下次成功构建会清除多余页面；失败保留之前的完整输出。关闭归档前应同时移除指向 `/archives/` 的导航，否则会因目标不存在而报错。

自定义主题通过 `pagination` 对象编写分页导航，通过 `archives` 数组编写年份归档。启用归档需要提供 `layouts/archives.html`，具体标记与样式由主题决定。

## Markdown 与主题参数

```toml
[markdown]
math = true
footnotes = true
admonitions = true

[extra]
show_toc = true
code_highlight = true
code_copy = true
math_render = true
footer_text = "感谢阅读"
```

`[markdown]` 决定正文的语法解析；`[extra]` 把自定义数据传给主题。默认主题使用上例中的主题参数，但其他主题可定义自己的参数。站点的 `[extra]` 覆盖主题 `theme.toml` 的默认值：表递归合并，数组和标量整体替换。

语法示例和样式调整见[技术写作](writing.md)；完整模板数据与参数规则见[主题开发](themes.md#主题参数)。`base_url` 的完整地址、路径限制和 RSS/sitemap 行为见[部署指南](deployment.md)。
