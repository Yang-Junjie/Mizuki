# 最小主题

这是一个可以直接构建的 Inja 主题，用少量 HTML 和一份 CSS 展示 Mizuki 的主题接口。可以从这里编写自己的主题，不需要修改 C++、安装前端工具或运行脚本。

在仓库根目录用文档内容验证：

```sh
./build/bin/mizuki build --config docs/mizuki.toml \
  --theme examples/minimal-theme --output build/minimal-theme --base-url /Mizuki/
```

文档内容全部为独立页面，因此本次构建的首页文章列表为空，页头会列出所有文档。博客中的文章、分页、归档、标签与分类由下面的对应模板处理。

用于自己的站点时，将这个目录复制到站点的 `themes/minimal/`，在站点配置里设置 `theme = "themes/minimal"`，然后运行 `mizuki serve`。不需要复制本仓库的站点配置或内容。

| 文件 | 展示的职责 |
| --- | --- |
| `layouts/base.html` | 继承入口；`site.pages` 生成全站页面导航，`site.posts` 提供最近三篇文章 |
| `layouts/home.html` | `posts` 只显示当前分页，`pagination` 提供前后页链接 |
| `layouts/post.html` | 当前文章的 `page` 元数据与 `html(page.content)` 正文 |
| `layouts/page.html` | 当前独立页面的 `page` 元数据与正文 |
| `layouts/archives.html` | 使用归档页的 `archives`，显示完整文章年份分组 |
| `layouts/taxonomy.html` | 用 `taxonomy` 与 `terms` 展示标签或分类总览及计数 |
| `layouts/term.html` | 用 `term`、`posts` 与 `pagination` 展示单个标签／分类的文章与分页 |
| `assets/style.css` | 完全由主题控制的样式，通过 `asset_base` 引用 |

所有入口均有 `site.posts` 和 `site.pages`，没有内容时是空数组。摘要含 `title`、`url`、`date`、`description`、`extra`、`tags`、`categories`，保留文稿自定义数据的类型，不携带全文。文章按日期降序、同日按输出路径升序；独立页面按输出路径升序。草稿不出现在集合里，当前文稿会出现在自己的集合中。

集合 URL 已相对当前输出页面计算并编码，直接使用即可。页头首页地址用 `url("/")`，适配部署前缀。`extra` 中自行定义的路径仍需通过 `url()` 等方式处理。仅在模板中读取全局数据不产生额外的输出文件。

文章根级 `tags = ["C++"]`、`categories = ["笔记"]` 自动生成相应总览和条目列表，未使用的类型不生成页面。`site.tags`、`site.categories` 提供全站 `{name, url, count}`；`page.tags`、`page.categories` 和摘要中的同名字段提供 `{name, url}` 链接。文章列表共用站点 `paginate_by`；分类采用扁平结构。主题可以自由改变这两个布局和文章中的链接标记。

本例专注于数据和布局，不加载代码高亮与公式脚本；Markdown HTML、脚注和章节链接仍可直接呈现。需要完整阅读组件、目录或主题参数时，参考 [默认主题](https://github.com/Yang-Junjie/Mizuki/tree/main/themes/default) 与[主题开发指南](../../docs/content/pages/themes.md)。
