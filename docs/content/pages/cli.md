+++
title = "命令行"
description = "创建站点和文稿，查询构建与预览参数，理解热重载、输出检查和失败恢复。"
+++

在终端运行 `mizuki --help` 查看命令，`mizuki --version` 查看程序版本；`-h` 和 `-V` 分别是它们的短选项。

## 命令速查

| 命令 | 用途 |
| --- | --- |
| `mizuki init DIRECTORY` | 在不存在或为空的目录创建站点 |
| `mizuki new PATH --title TEXT` | 在内容目录中创建 Markdown 文稿 |
| `mizuki build` | 执行一次完整构建 |
| `mizuki watch` | 监听变化并重建 |
| `mizuki serve` | 监听变化、提供本地 HTTP 预览并刷新浏览器 |

## 创建站点

```sh
mizuki init my-blog
cd my-blog
mizuki serve
```

`init` 生成 `mizuki.toml`、`content/posts/welcome.md` 示例文章、`content/pages/about.md` 关于页面、可编辑的 `themes/default/` 和忽略 `/dist/` 的 `.gitignore`。生成的配置启用分页和归档，并提供首页、关于和归档导航，立即运行 `build` 或 `serve` 即可使用。

目标目录必须不存在或为空；`mizuki init .` 可在空的当前目录初始化。非空目录（包括隐藏文件）、普通文件和指向目标目录的符号链接都会被拒绝，已有文件不覆盖。嵌套目录可以自动创建；操作失败时清理本次创建的文件和目录，保留已有目录及其他文件。


## 创建文稿

`new` 的路径相对于配置中的内容目录，无需再写 `content/` 前缀；父目录自动创建。`--title` 必填，`--description` 可选，均为单行纯文本，保留引号、反斜杠及首尾空白，拒绝空白标题、换行、NUL 等无效字符。生成的 front matter 使用 `+++` 包裹的 TOML，由 toml++ 正确处理引号与转义。日期使用创建时的本地日期 `YYYY-MM-DD`，之后可以自行编辑。

```sh
mizuki new posts/another.md --title "另一篇文章" --description "文章摘要"
mizuki new posts/draft.md --title "还在写作" --draft
mizuki new pages/contact.md --title "联系我"
mizuki new notes/first.md --title "笔记" --config sites/blog.toml
mizuki new posts/first.md --title "指定内容目录" --content manuscripts
```

默认生成 `draft = false`，可以立即出现在预览中；`--draft` 生成草稿，`build`、`watch` 和 `serve` 都跳过它。`pages/` 下的文稿沿用独立页面规则，不进入文章列表。`new` 支持 `--config` 和 `--content`，路径基准与构建命令一致。

新文件必须使用 `.md` 扩展名，路径不能是绝对路径、包含 `..` 或经过内容目录内的符号链接，顶层 `index.md` 保留给自动生成的首页。已有文件、目录和悬空符号链接均不会被替换。以 `-` 开头的路径可以放在 `--` 后，例如 `mizuki new --title "文章" -- -post.md`。运行 `mizuki init --help` 或 `mizuki new --help` 可查看对应参数。

## 构建与预览参数

| 参数 | 适用命令 | 说明 |
| --- | --- | --- |
| `--config FILE` | `build`、`watch`、`serve`、`new` | 指定站点配置 |
| `--content DIR` | `build`、`watch`、`serve`、`new` | 覆盖内容目录 |
| `--theme DIR` | `build`、`watch`、`serve` | 覆盖主题目录 |
| `--output DIR` | `build`、`watch`、`serve` | 覆盖输出目录 |
| `--base-url URL` | `build`、`watch`、`serve` | 覆盖发布地址或部署路径 |
| `--port PORT` | `serve` | 预览端口，默认 `3939`，范围 1–65535 |

```sh
mizuki build --config sites/blog.toml
mizuki watch --theme themes/custom
mizuki serve --port 4000
mizuki serve --base-url /blog/
```

`serve` 仅监听 `127.0.0.1`，按 Ctrl+C 退出。默认访问 `http://127.0.0.1:3939/`；配置 `/blog/` 部署路径时访问 `http://127.0.0.1:3939/blog/`。端口占用会报错退出。

命令行值覆盖配置文件，相对路径按当前工作目录解析。配置文件中相对目录的基准与内置默认值见[站点配置](configuration.md#优先级与路径基准)。

## 监听与配置热重载

`watch` 和 `serve` 监听内容、主题及配置文件。修改配置会重新构建；通过临时文件替换配置的保存方式也受支持。切换 `content` / `theme` 后会改为监听新目录，切换 `output` 后预览服务在构建成功时改为读取新输出。命令行覆盖在每次重载后依然有效。新目录必须已经存在且可访问。

运行中的配置错误或构建失败会打印诊断，保留最近一次成功的输出和预览，并继续监听以便修复；构建成功才通知浏览器刷新。新输入目录有效、但其中内容导致构建失败时，仍会监听这些新目录，修正文稿即可恢复。启动时配置无效或无法建立监听则退出；首次 `serve` 构建失败且没有已有输出时也会退出。

默认配置被删除时恢复内置默认值并保留命令行覆盖；显式 `--config` 文件被删除则报错并等待它重新创建。切换输出目录不会主动删除旧输出目录。

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
