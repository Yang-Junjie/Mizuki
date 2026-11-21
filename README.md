# Mizuki

一个用 C++ 编写的轻量静态博客框架，支持 Markdown 文章、独立页面、TOML 站点配置、本地预览和自动刷新。

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

## 站点配置

默认读取当前工作目录的 `mizuki.toml`，文件不存在时沿用内置默认值。仓库自带可直接运行的配置：

```toml
title = "Mizuki"
description = "一个用 C++ 编写的轻量博客框架"
author = ""
language = "zh-CN"
content = "content"
theme = "themes/default"
output = "dist"

[[navigation]]
title = "首页"
url = "/"

[[navigation]]
title = "关于"
url = "/pages/about.md"
```

支持的顶层键只有以上八项。除 `navigation` 外均为字符串；`description` 和 `author` 允许为空，其余不能为空或全是空白。导航项只能包含非空字符串 `title` 和 `url`。省略导航时默认只有首页，`navigation = []` 隐藏导航。未知键、类型错误、重复键或 TOML 语法错误都会报错，并给出配置文件位置。

所有命令支持指定配置及目录：

```sh
./build/bin/mizuki build --config sites/blog.toml
./build/bin/mizuki serve --config sites/blog.toml --output preview
./build/bin/mizuki watch --content content --theme themes/default --output dist
```

优先级为 **命令行 > 配置文件 > 内置默认值**，参数先后顺序不影响优先级。配置文件中的相对目录，以及已存在配置文件中省略的目录默认值，都以配置文件所在目录为基准；命令行相对路径和 `--config` 路径以当前工作目录为基准。显式指定的 `--config` 文件不存在时会报错。

`watch` 和 `serve` 监听内容、主题及配置文件。修改配置会重新构建；通过临时文件替换配置的保存方式也受支持。切换 `content` / `theme` 后会改为监听新目录，切换 `output` 后预览服务在构建成功时改为读取新输出。命令行覆盖在每次重载后依然有效。新目录必须已经存在且可访问。

运行中的配置错误或构建失败会打印诊断，保留最近一次成功的输出和预览，并继续监听以便修复；构建成功才通知浏览器刷新。新输入目录有效、但其中内容导致构建失败时，仍会监听这些新目录，修正文稿即可恢复。启动时配置无效或无法建立监听则退出；首次 `serve` 构建失败且没有已有输出时也会退出。

默认配置被删除时恢复内置默认值并保留命令行覆盖；显式 `--config` 文件被删除则报错并等待它重新创建。切换输出目录不会主动删除旧输出目录。

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
draft: false
---

正文……
```

- `title` 必填且不能为空。
- 普通文章必须有非空 `date`；按原样输出，不做日期格式校验。首页按日期字符串降序排列，相同日期按输出 URL 排序，建议使用 `YYYY-MM-DD`。
- 内容根目录下 `pages/` 内的 Markdown 是独立页面，使用 `layouts/page.html`，允许省略 `date`；若提供日期仍不能为空。独立页面不进入首页文章列表，可以加入导航。
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

改写仅作用于真实链接的 `href`，包括原始 HTML 的 `<a href="...md">`；普通文字、代码、图片的 `src`、HTML 注释和 script/style/textarea/title 内容不改写。外部链接、纯锚点和普通非 Markdown 资源链接保持原样。不会验证 fragment 对应的锚点，也不会自动为标题生成 ID。

## 主题模板

| 模板 | 必需占位符 | 用途 |
| --- | --- | --- |
| `layouts/home.html` | `{asset_base}`、`{posts}` | 生成首页；不读取 `content/index.md` |
| `layouts/post.html` | `{markdown}`、`{asset_base}`、`{title}`、`{date}` | 普通文章 |
| `layouts/page.html` | `{markdown}`、`{asset_base}`、`{title}` | 独立页面；没有已发布独立页面时无需此模板 |

`{asset_base}` 是相对当前页面的主题资源目录，包含结尾 `/`；`{posts}` 是已发布文章的 HTML 列表。文章和独立页面都可使用 `{date}`，独立页面省略日期时替换为空字符串。

所有模板还可选用 `{site_title}`、`{site_description}`、`{site_author}`、`{site_language}` 和 `{navigation}`。新占位符均可省略，现有主题仍可使用。文本和属性值会转义 HTML；正文、文章列表和导航作为 HTML 插入。替换值中的占位符不会再次替换。

主题 `assets/` 中的文件复制到输出 `assets/`，内容目录的非 Markdown 文件按相对路径复制。

## 路径冲突与输出管理

构建前统一检查所有页面、生成的首页、内容静态文件和主题资源的输出路径。多个输入占用同一位置会直接报错，列出目标和两个来源，例如：

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

测试均使用临时目录，包括构建与输出事务、冲突诊断和旧输出保护、Markdown 链接、TOML 校验及优先级、独立页面和导航，以及真实 `watch` / `serve` 进程的文件变化、配置热重载、监听目录切换、HTTP 和 SSE。测试进程退出时会清理临时文件及子进程。

代码分层、接口约定、格式化和单独运行测试的方法见 [开发约定](CONTRIBUTING.md)。
