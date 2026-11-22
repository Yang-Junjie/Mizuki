# Mizuki

一个用 C++ 编写的轻量静态博客框架，支持站点初始化、文章创建、Markdown 文章、独立页面、TOML 站点配置、本地预览和自动刷新。

## 编译与运行

需要 CMake 3.28 或更新版本、支持 C++23 的编译器和 Git。启用测试还需要 Python 3；测试只使用 Python 标准库。

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure

# 在项目根目录构建、持续构建，或启动预览
./build/bin/mizuki build
./build/bin/mizuki watch
./build/bin/mizuki serve
./build/bin/mizuki serve --port 4000
```

默认预览地址是 `http://127.0.0.1:3939`，按 Ctrl+C 退出。`--port` 仅用于 `serve`，范围为 1–65535；端口占用会报错退出。

CMake 使用 spdlog、md4c、efsw、cpp-httplib 和 toml++。依赖版本在 `cmake/third_party_*.cmake` 中固定；可用的 spdlog、md4c 和 toml++ 安装包会优先被查找，其余通过 FetchContent 下载，首次配置需要网络。加入 `-DFETCHCONTENT_TRY_FIND_PACKAGE_MODE=NEVER` 可全部使用项目固定的源码依赖。不需要测试时可设 `-DBUILD_TESTING=OFF`，无需 Python。

Linux 安装 Ninja 后，也可使用 `linux-debug` / `linux-release` presets：

```sh
cmake --preset linux-debug
cmake --build --preset linux-debug
ctest --preset linux-debug
./build/linux-debug/bin/mizuki serve
```

Windows 可在 MSVC 开发者终端中使用 Ninja 编译，运行 `build\bin\mizuki.exe`；使用 Visual Studio 多配置生成器时，需带 `--config Release` / `ctest -C Release`，程序位于 `build\bin\Release\mizuki.exe`。原有 MSVC、ClangCL presets 保留，其工具链需与本机安装匹配。GitHub Actions 使用 Linux GCC 和 Windows MSVC + Ninja 构建并运行完整测试；具体平台是否通过以工作流结果为准。

## 创建站点与文章

编译后可安装命令行程序，再将安装目录的 `bin` 加入 `PATH`。默认主题已编入程序，初始化无需访问源码目录或下载主题：

```sh
cmake --install build --prefix ./build/install --component mizuki
export PATH="$PWD/build/install/bin:$PATH"
```

上面的 `export` 适用于 Linux/macOS shell；Windows 将对应安装目录的 `bin` 加入 `Path`。使用 Visual Studio 多配置构建时，安装也需带 `--config Release`。

之后可以在任意工作目录创建独立站点：

```sh
mizuki init my-blog
cd my-blog
mizuki new posts/hello.md --title "第一篇文章"
mizuki serve
```

`init` 生成 `mizuki.toml`、`content/posts/welcome.md` 示例文章、`content/pages/about.md` 关于页面、可编辑的 `themes/default/` 和忽略 `/dist/` 的 `.gitignore`。生成的配置启用分页和归档，并提供首页、关于和归档导航，立即运行 `build` 或 `serve` 即可使用。

目标目录必须不存在或为空；`mizuki init .` 可在空的当前目录初始化。非空目录（包括隐藏文件）、普通文件和指向目标目录的符号链接都会被拒绝，已有文件不覆盖。嵌套目录可以自动创建；操作失败时清理本次创建的文件和目录，保留已有目录及其他文件。

`new` 的路径相对于配置中的内容目录，无需再写 `content/` 前缀；父目录自动创建。`--title` 必填，`--description` 可选，均为单行纯文本，保留引号、反斜杠及首尾空白，拒绝空白标题、换行、NUL 等无效字符。生成的 front matter 符合 Mizuki 的简单字面值规则，不按 YAML 转义。日期使用创建时的本地日期 `YYYY-MM-DD`，之后可以自行编辑。

```sh
mizuki new posts/another.md --title "另一篇文章" --description "文章摘要"
mizuki new posts/draft.md --title "还在写作" --draft
mizuki new pages/contact.md --title "联系我"
mizuki new notes/first.md --title "笔记" --config sites/blog.toml
mizuki new posts/first.md --title "指定内容目录" --content manuscripts
```

默认生成 `draft: false`，可以立即出现在预览中；`--draft` 生成草稿，`build`、`watch` 和 `serve` 都跳过它。`pages/` 下的文稿沿用独立页面规则，不进入文章列表。`new` 支持 `--config` 和 `--content`，路径基准与构建命令一致。

新文件必须使用 `.md` 扩展名，路径不能是绝对路径、包含 `..` 或经过内容目录内的符号链接，顶层 `index.md` 保留给自动生成的首页。已有文件、目录和悬空符号链接均不会被替换。以 `-` 开头的路径可以放在 `--` 后，例如 `mizuki new --title "文章" -- -post.md`。运行 `mizuki init --help` 或 `mizuki new --help` 可查看对应参数。

## 站点配置

默认读取当前工作目录的 `mizuki.toml`，文件不存在时沿用内置默认值。仓库自带可直接运行的配置：

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

支持的顶层键只有上例中的项目。`paginate_by` 必须是非负整数，`archives` 必须是布尔值；其余除 `navigation` 外均为字符串。字符串字段中，`description` 和 `author` 允许为空，其余不能为空或全是空白。导航项只能包含非空字符串 `title` 和 `url`。省略导航时默认只有首页，`navigation = []` 隐藏导航。未知键、类型错误、重复键或 TOML 语法错误都会报错，并给出配置文件位置。

`build`、`watch` 和 `serve` 支持指定配置及目录；`new` 支持其中的 `--config` 和 `--content`：

```sh
./build/bin/mizuki build --config sites/blog.toml
./build/bin/mizuki serve --config sites/blog.toml --output preview
./build/bin/mizuki watch --content content --theme themes/default --output dist
./build/bin/mizuki build --base-url https://example.com/blog/
```

优先级为 **命令行 > 配置文件 > 内置默认值**，参数先后顺序不影响优先级。配置文件中的相对目录，以及已存在配置文件中省略的目录默认值，都以配置文件所在目录为基准；命令行相对路径和 `--config` 路径以当前工作目录为基准。显式指定的 `--config` 文件不存在时会报错。

`watch` 和 `serve` 监听内容、主题及配置文件。修改配置会重新构建；通过临时文件替换配置的保存方式也受支持。切换 `content` / `theme` 后会改为监听新目录，切换 `output` 后预览服务在构建成功时改为读取新输出。命令行覆盖在每次重载后依然有效。新目录必须已经存在且可访问。

运行中的配置错误或构建失败会打印诊断，保留最近一次成功的输出和预览，并继续监听以便修复；构建成功才通知浏览器刷新。新输入目录有效、但其中内容导致构建失败时，仍会监听这些新目录，修正文稿即可恢复。启动时配置无效或无法建立监听则退出；首次 `serve` 构建失败且没有已有输出时也会退出。

默认配置被删除时恢复内置默认值并保留命令行覆盖；显式 `--config` 文件被删除则报错并等待它重新创建。切换输出目录不会主动删除旧输出目录。

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

完整的 GitHub Pages 配置和工作流示例见 [部署指南](docs/deployment.md)。

## 内容与独立页面

```text
content/
  posts/hello.md           -> dist/posts/hello.html，进入文章列表
  notes/another.md         -> dist/notes/another.html，进入文章列表
  pages/about.md           -> dist/pages/about.html，独立页面
  images/photo.png         -> dist/images/photo.png，原样复制
  index.md                 -> 忽略，首页由主题生成
```

每个待处理的 Markdown 文件必须以 `---` 包裹的 front matter 开头：

```md
---
title: 第一篇文章
date: 2026-11-15
description: 一段用于页面描述和 RSS 摘要的介绍。
draft: false
---

正文……
```

- `title` 必填且不能为空。
- 普通文章必须有有效的 `date`，严格使用 `YYYY-MM-DD`，年份为 0001–9999，并校验闰年与每月天数。首页、归档与 RSS 按日期降序排列，相同日期按输出 URL 排序。未来日期仍会发布，不做定时发布处理。原先使用任意日期文本的文章需要先改为此格式。
- 内容根目录下 `pages/` 内的 Markdown 是独立页面，使用 `layouts/page.html`，允许省略 `date`；若提供日期则使用同一套格式与日历校验。独立页面不进入首页文章列表，可以加入导航。
- `description` 是可选的单行纯文本，可用成对的单引号或双引号包裹。默认主题用它作为页面的 meta description，省略或为空时回退到站点 `description`；RSS 仅在文章自身有描述时输出摘要，不从正文自动截取。
- `draft: true` 跳过页面，不生成 HTML，不占用输出路径。草稿仍需有合法的 front matter。
- 键名重复、缺少结束 `---` 或格式错误会导致构建失败。Front matter 只支持简单 `key: value` 行，不支持 YAML 注释、列表或嵌套结构；站点配置单独使用 TOML。

### 导航与文内链接

导航的本地路径始终从站点根目录解析，可以用 `/`、`/pages/about.md` 或 `pages/about.html`。构建时会转换为当前页面可用的相对链接。导航目标必须是实际发布的页面或静态文件；外部导航允许 `http://`、`https://` 和 `//` 地址。

Markdown 文内的本地 `.md` 链接会改写为目标 `.html`，支持相对路径、根路径、引用式链接、URL 编码的中文或空格文件名，并保留 query 和 fragment：

```md
[关于](../pages/about.md?from=article#intro)
[首页](/index.md)
```

在 `posts/hello.md` 中，前一条输出为 `../pages/about.html?from=article#intro`。链接解析使用实际发布页面清单，路径及大小写应与源文件一致。目标缺失、指向草稿或越出内容目录会报错，避免发布失效的 Markdown 链接。根 `/index.md` 固定指向生成的首页。

`.md` 到 `.html` 的改写仅作用于真实链接的 `href`，包括正文中原始 HTML 的 `<a href="...md">`。图片和其他资源中的 `.md` 不转换为页面链接。

正文的根路径资源也会带上部署前缀：包括 `img`、`source`、`script`、`audio`、`video`、`iframe`、`input`、`track`、`embed` 的 `src`，`link` 的 `href`、`video` 的 `poster` 和 `object` 的 `data`。相对资源路径保持原样。普通文字、代码、HTML 注释和 script/style/textarea/title 的文本内容不改写。不会验证 fragment 对应的锚点，也不会自动为标题生成 ID。

URL 改写只处理 Markdown 渲染出的正文。原样复制的 HTML、CSS、模板中写死的地址和 `srcset` 不自动改写：模板使用 `{asset_base}` 或 `{base_path}`，CSS 的 `url(...)` 相对样式文件书写，`srcset` 相对当前页面书写。

## RSS、sitemap 与规范地址

将 `base_url` 设置为包含域名的完整 HTTP(S) 发布地址即可启用：

```toml
base_url = "https://example.com/blog/"
```

成功构建后会生成 `dist/rss.xml` 和 `dist/sitemap.xml`。输出目录仍不包含 `blog/` 前缀：RSS、sitemap 和默认主题的 canonical 地址统一使用完整发布地址，并对中文、空格和特殊字符进行 URL 编码。

- RSS 2.0 只包含已发布文章，使用标题、发布日期、可选描述和页面完整地址；文章地址也作为稳定的 GUID。独立页面、草稿和原样复制的 HTML 不进入订阅源。
- 发布日期只有日历日期，RSS 将其明确表示为当天 `00:00:00 GMT`，不依赖本机时区或语言环境。订阅源不写入当前构建时间，相同输入的输出保持一致。
- sitemap 包含生成的首页、已发布文章和独立页面。首页使用站点根地址，其余页面使用实际 `.html` 地址；不将发布日期或文件时间冒充最后修改时间。当前生成单个文件，最多 50,000 个地址、50 MiB，单个地址少于 2,048 字符；超限会报错，不自动拆分。
- 默认主题在首页、文章和独立页面的 `<head>` 中加入 canonical 和 RSS 自动发现链接，在页脚显示 RSS 入口。自定义主题可使用下表中的可选占位符。

当 `base_url` 只是 `/` 或 `/blog/` 时，正常构建和预览，并打印说明；不生成这两个 XML 文件，也不插入 canonical 和订阅链接。切换配置后，下一次成功构建会移除旧的生成文件。没有完整发布地址时，内容目录里自行提供的同名 XML 仍可原样复制。

启用生成后，`rss.xml`、`sitemap.xml` 是受管理的输出路径，与内容文件冲突时会明确报出生成器和输入来源。生成失败会保留之前的完整站点。

## 首页分页与文章归档

```toml
paginate_by = 10
archives = true
```

`paginate_by` 设置每页文章数，内置默认值 `0` 表示不分页；`archives` 控制年份归档，内置默认值为 `false`。仓库示例显式启用这两项，已有配置省略时保留原有输出。两项相互独立，也可以只启用其中一项。

首页仍位于 `/`，第二页起使用 `/page/2/`、`/page/3/`，对应输出 `page/2/index.html` 等文件。不额外生成 `/page/1/`。空站点也有一张首页；文章数不超过每页数量时不生成额外分页，也不显示分页导航。默认主题在后续分页的浏览器标题中加入页码。

启用归档后生成 `/archives/`，对应输出 `archives/index.html`，按年份降序分组，列出全部已发布文章；同一年内沿用日期降序、同日按输出 URL 排序。草稿、独立页面和复制的 HTML 文件不进入文章列表。没有文章时归档显示空状态。可在导航中加入入口：

```toml
[[navigation]]
title = "归档"
url = "/archives/"
```

分页和归档的文章链接、主题资源和主导航都相对当前页面生成，支持根目录与子路径部署。配置完整 `base_url` 时，每页 canonical 使用对应的目录地址，sitemap 收录首页、后续分页、归档以及内容页面；RSS 始终使用全部已发布文章，不受分页数量影响。

分页输出与归档也参与构建前的路径冲突检查。文章删除、改为草稿、调大分页数量或关闭功能后，下次成功构建会清除多余页面；失败保留之前的完整输出。关闭归档前应同时移除指向 `/archives/` 的导航，否则会因目标不存在而报错。

自定义主题需要在 `layouts/home.html` 放入 `{pagination}` 才能显示分页导航。只有实际产生多页时，这个占位符才是必需的；缺失时构建明确报错，避免旧文章失去入口。启用归档还需要提供 `layouts/archives.html`，其中必须包含 `{archives}` 和 `{asset_base}`。默认关闭这两项时，旧主题无需调整。

## 主题模板

| 模板 | 必需占位符 | 用途 |
| --- | --- | --- |
| `layouts/home.html` | `{asset_base}`、`{posts}`；实际多页时还需 `{pagination}` | 首页及后续分页；不读取 `content/index.md` |
| `layouts/archives.html` | `{asset_base}`、`{archives}` | 按年份分组的归档；仅启用归档时需要 |
| `layouts/post.html` | `{markdown}`、`{asset_base}`、`{title}`、`{date}` | 普通文章 |
| `layouts/page.html` | `{markdown}`、`{asset_base}`、`{title}` | 独立页面；没有已发布独立页面时无需此模板 |

`{asset_base}` 是相对当前页面的主题资源目录，包含结尾 `/`；`{posts}` 是当前页的已发布文章 HTML 列表；`{archives}` 是完整的年份分组 HTML。首页模板还可选用 `{title}`（第一页为站点标题，后续页附加页码）、`{page_number}` 和 `{page_count}`；页码从 1 开始。`{pagination}` 是完整的分页导航 HTML，只有一页时为空。文章和独立页面都可使用 `{date}`，独立页面省略日期时替换为空字符串。

所有模板还可选用 `{site_title}`、`{site_description}`、`{site_author}`、`{site_language}`、`{navigation}`、`{base_url}` 和 `{base_path}`。`{base_url}` 是规范化后的完整配置地址，`{base_path}` 是规范化后的部署路径，两者都包含结尾 `/`。这些站点占位符均可省略，现有主题仍可使用。文本和属性值会转义 HTML；正文、文章列表和导航作为 HTML 插入。替换值中的占位符不会再次替换。

发布信息还提供以下可选占位符，旧主题无需修改即可继续使用：

| 占位符 | 内容 |
| --- | --- |
| `{description}` | 首页、后续分页和归档使用站点描述；文章与独立页面优先使用自身描述，为空时回退到站点描述。作为文本转义 |
| `{canonical_url}` | 当前页面的完整规范地址；首页为站点根地址。未配置域名时为空，作为文本转义 |
| `{feed_url}` | RSS 的完整地址；未配置域名时为空，作为文本转义 |
| `{publication_head}` | 完整的 canonical 和 RSS 自动发现 `<link>` 标签，放入 `<head>`；未配置域名时为空 |
| `{feed_link}` | 完整的 RSS `<a>` 标签，可放入页脚；未配置域名时为空 |

主题 `assets/` 中的文件复制到输出 `assets/`，内容目录的非 Markdown 文件按相对路径复制。

## 路径冲突与输出管理

构建前统一检查所有内容页面、首页及分页、启用的归档、RSS 与 sitemap、内容静态文件和主题资源的输出路径。多个输入占用同一位置会直接报错，列出目标和两个来源，例如：

```text
Output path conflict
  Output: dist/posts/post.html
  Source: content/posts/post.html
  Source: content/posts/post.md
```

同样会拒绝 `content/index.html` 与生成首页冲突、内容资源与主题资源同名，以及文件与目录占用同一路径（如文件 `assets` 和资源 `assets/css/style.css`）。ASCII 字母仅大小写不同的目标也视为冲突，使构建结果可移至大小写不敏感的文件系统。资源没有隐式覆盖优先级；发现冲突后应重命名或移除其中一个来源。

`build`、`watch` 和 `serve` 都使用全量构建。先将页面和资源写到输出目录旁的临时目录，全部成功后再替换正式输出。因此删除、重命名文章、改为草稿或删除资源后，下一次成功构建会移除旧产物；解析、链接、渲染或复制失败时保留之前的输出。

**输出目录完全由 Mizuki 管理。** 手动放入的文件也会在下一次成功构建时消失。需要发布的文件应放入内容目录或主题 `assets/`。输出目录不能与内容或主题目录重叠，不能包含配置文件，不能是文件系统根目录、当前工作目录或其祖先，也不能是符号链接或普通文件。

替换时先备份旧输出，再将临时目录重命名为正式输出；替换失败会尝试恢复备份。如果恢复也失败，错误信息会给出保留旧站点的备份路径。临时目录或备份清理失败时会打印警告及残留路径。

目录替换期间可能短暂无法访问输出路径。本机制不保证进程崩溃后的自动恢复，也不支持多个进程同时构建同一输出目录。

预览只监听本机地址；HTML 响应会注入 SSE 自动刷新脚本，静态产物不包含该脚本。浏览器断线后会自动重连；构建失败不触发刷新。

## 回归测试

```sh
cmake -S . -B build -DBUILD_TESTING=ON
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
```

测试均使用临时目录，包括仓库外初始化与写作、已有文件保护和创建失败清理、构建与输出事务、冲突诊断和旧输出保护、Markdown 链接、TOML 校验及优先级、根目录与子目录部署、独立页面和导航、日期与描述校验、RSS/sitemap XML、canonical 与失败回滚，以及真实 `watch` / `serve` 进程的文件变化、配置热重载、监听目录切换、HTTP、SSE 及部署路径切换。测试进程退出时会清理临时文件及子进程。

代码分层、接口约定、格式化和单独运行测试的方法见 [开发约定](CONTRIBUTING.md)。
