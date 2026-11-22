+++
title = "内容与链接"
description = "查阅文稿元数据、文章与页面规则、站内链接、标题锚点和目录。"
+++

先按[快速开始](getting-started.md)创建站点，再用本页查阅内容文件与链接的处理规则。代码、公式、脚注与标签分类的写法见[技术写作](writing.md)。

## 内容目录与文稿元数据

```text
content/
  posts/hello.md           -> dist/posts/hello.html，进入文章列表
  notes/another.md         -> dist/notes/another.html，进入文章列表
  pages/about.md           -> dist/pages/about.html，独立页面
  images/photo.png         -> dist/images/photo.png，原样复制
  index.md                 -> 忽略，首页由主题生成
```

每个待处理的 Markdown 文件必须以 front matter 开头。`init` 与 `new` 生成 `+++` 包裹的 TOML：

```toml
+++
title = "第一篇文章"
date = "2026-11-22"
description = "一段用于页面描述和 RSS 摘要的介绍。"
draft = false
tags = ["C++", "图形学"]
categories = ["技术笔记"]

[extra]
cover = "images/cover.webp"
cover_alt = "文章封面"
show_toc = false
labels = ["C++", "图形学"]

[extra.author]
name = "Artichoke"
+++
```

结束的 `+++` 后是 Markdown 正文。内置字段规则如下：

- `title` 必填且必须为非空字符串。
- 普通文章必须有有效的 `date`，使用 `YYYY-MM-DD` 字符串或 TOML 本地日期，年份为 0001–9999，并校验闰年与每月天数。首页、归档与 RSS 按日期降序排列，相同日期按输出 URL 排序。未来日期仍会发布，不做定时发布处理。
- 内容根目录下 `pages/` 内的 Markdown 是独立页面，使用 `layouts/page.html`，允许省略 `date`；若提供日期则使用同一套格式与日历校验。独立页面不进入首页文章列表，可以加入导航。
- `description` 是可选字符串。默认主题用它作为页面的 meta description，省略或为空时回退到站点 `description`；RSS 仅在文章自身有描述时输出摘要，不从正文自动截取。
- `layout = "special/project.html"` 可为文章或独立页面选择主题 `layouts/` 下的模板，省略时分别使用 `post.html` 或 `page.html`。
- `tags`、`categories` 是可选字符串数组，仅用于文章，分别生成标签和分类聚合页；详见[技术写作](writing.md#标签与分类)。
- `draft = true` 跳过页面，不生成 HTML，不占用输出路径。草稿仍需有合法的 front matter。
- TOML 根级只允许上述字段与 `extra`；未知键、重复键、字段类型错误和格式错误会中止构建，并保留上次成功输出。

`[extra]` 支持字符串、布尔值、有限数值、数组、嵌套表与表数组，供模板通过 `page.extra` 读取。首页分页的 `post.extra` 与归档中的文章摘要保留相同数据。未设置时为空对象，页面数据不与 `site.extra` 隐式合并；模板可用 `default(page.extra.show_toc, site.extra.show_toc)` 定义回退。

默认主题通过页面 `show_toc` 覆盖站点目录开关，并在详情页、首页列表与归档显示 `cover`。封面路径从站点根目录解析，例如图片放在 `content/images/cover.webp`，写 `cover = "images/cover.webp"`；也支持外部图片 URL。`url(page.extra.cover)` 负责 URL 编码与部署子路径，`cover_alt` 是可选说明。

已有的 `---` 简单 `key: value` 文稿继续支持；它不是完整 YAML。`tags` 和 `categories` 可使用行内 TOML 数组，例如 `tags: ["C++", "图形学"]`，不支持 YAML 的多行列表或嵌套数据。需要自定义参数时改用 TOML，详见[主题开发指南](themes.md)。


## 站内链接与资源


导航的本地路径始终从站点根目录解析，可以用 `/`、`/pages/about.md` 或 `pages/about.html`。构建时会转换为当前页面可用的相对链接。导航目标必须是实际发布的页面或静态文件；外部导航允许 `http://`、`https://` 和 `//` 地址。

Markdown 文内的本地 `.md` 链接会改写为目标 `.html`，支持相对路径、根路径、引用式链接、URL 编码的中文或空格文件名，并保留 query 和 fragment：

```md
[关于](../pages/about.md?from=article#intro)
[首页](/index.md)
```

在 `posts/hello.md` 中，前一条输出为 `../pages/about.html?from=article#intro`。链接解析使用实际发布页面清单，路径及大小写应与源文件一致。目标缺失、指向草稿或越出内容目录会报错，避免发布失效的 Markdown 链接。根 `/index.md` 固定指向生成的首页。

`.md` 到 `.html` 的改写仅作用于真实链接的 `href`，包括正文中原始 HTML 的 `<a href="...md">`。图片和其他资源中的 `.md` 不转换为页面链接。

正文的根路径资源也会带上部署前缀：包括 `img`、`source`、`script`、`audio`、`video`、`iframe`、`input`、`track`、`embed` 的 `src`，`link` 的 `href`、`video` 的 `poster` 和 `object` 的 `data`。相对资源路径保持原样。普通文字、代码、HTML 注释和 script/style/textarea/title 的文本内容不改写。fragment 保留原样，不验证对应锚点是否存在；标题 ID 的生成规则见下节。

URL 改写只处理 Markdown 渲染出的正文。原样复制的 HTML、CSS、模板中写死的地址和 `srcset` 不自动改写：模板使用 `{{ asset_base }}` 或 `{{ site.base_path }}`，CSS 的 `url(...)` 相对样式文件书写，`srcset` 相对当前页面书写。

## 标题锚点与文章目录

文章与独立页面的正文标题会自动获得 ID，并在标题末尾显示可直接打开或分享的 `#` 章节链接。ATX 标题（`#` 至 `######`）和 Setext 标题使用相同规则；代码块、HTML 注释及 script/style/textarea/title 等原始文本元素中的内容不参与标题识别。

ID 按标题的实际文字生成，强调、行内代码和链接保留原有格式，图片使用替代文字。HTML 实体先解码，连续空白（包括全角空格）合并。生成规则为：

- ASCII 大写字母转为小写，保留字母、数字、`-`、`_` 和非 ASCII 字符（包括中文及中文标点）；不做 Unicode 大小写转换或规范化。
- 单词间空白转为 `-`，其余 ASCII 标点移除。例如 ``## *安装* 与 `C++` `` 得到 `安装-与-c`，`## A & B` 得到 `a-b`。
- 相同 ID 按正文顺序添加 `-2`、`-3` 等后缀，并避开当前正文中已经使用的 ID。例如两处 `## 安装` 分别得到 `安装` 和 `安装-2`。
- 空标题或只含被移除标点的标题使用 `section`，重复时同样添加后缀。手写 HTML 的 `h1` 至 `h6` 也参与标题处理，保留已有的非空 ID；新 ID 会避开正文中所有手写 ID，包括位于后面的 ID。作者提供的重复 ID 不会自动修正。

ID 仅在当前页面内分配，相同输入重复构建得到相同结果。标题文字或同名标题顺序改变后，应相应更新指向它的链接。章节链接使用 URL 编码，HTML 的 ID 保留原始文字。例如：

```md
## 安装

[本页安装说明](#安装)
[另一篇文章的章节](../pages/about.md#安装)
```

默认主题在文章与独立页面正文前展示目录，仅列出二、三级标题。三级标题嵌套于前面的二级标题；没有所属二级标题时作为顶层条目，一级标题会结束前一个二级标题的归属范围。没有二、三级标题时不输出目录。空标题在目录中显示为“（无标题）”。

自定义主题通过 `toc` 标题树与 `page.headings` 数组自行组织目录，也可使用默认主题的 `{% include "partials/toc.html" %}`。默认目录使用原生 `<details open>`，默认展开，可用鼠标或键盘折叠，关闭 JavaScript 仍能跳转。可通过 `.toc` 和 `.heading-anchor` 调整样式，或在站点 `[extra]` 中设置 `show_toc = false` 隐藏目录。

原样复制的 HTML 文件不参与此处理。站内 Markdown 链接继续只检查目标页面是否发布，不检查 fragment 是否存在。

站点导航的配置见[站点配置](configuration.md#导航)，部署前缀的选择见[部署指南](deployment.md#部署地址与子路径)。
