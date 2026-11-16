# Mizuki

一个用 C++ 编写的轻量博客框架。

编译后，在项目根目录构建示例站点：

```sh
./build/bin/mizuki build
```

写作时可以使用 `watch`，启动后先构建一次，再监听内容和主题目录中的变化，自动重新构建。按 Ctrl+C 退出：

```sh
./build/bin/mizuki watch
```

使用 `serve` 可以同时启动本地预览和自动构建，默认访问 `http://127.0.0.1:3939`。修改文章、模板或静态资源后，构建成功会自动刷新已打开的页面，按 Ctrl+C 退出：

```sh
./build/bin/mizuki serve
./build/bin/mizuki serve --port 4000
```

自动刷新通过 SSE 推送构建通知，浏览器断线后会自动重连。刷新脚本只加入预览服务返回的 HTML，不修改主题或 `dist` 文件；构建失败时不会触发刷新。

所有命令都支持指定目录，相对路径以执行命令时所在的目录为准：

```sh
./build/bin/mizuki watch --content content --theme themes/default --output dist
```

`--port` 仅用于 `serve`，范围为 1–65535，服务只监听本机地址。端口占用时会打印错误并退出。

## Front Matter

每个 Markdown 文件必须以 `---` 包裹的 front matter 开头：

```md
---
title: 第一篇文章
date: 2026-11-15
draft: false
---

正文……
```

- `title` 必填，缺失或为空会导致该页构建失败。
- `date` 必填，按原样输出到页头，不做格式校验。
- `draft: true` 的页面会被跳过，不生成 HTML。
- 键名重复、缺少结束 `---` 或格式错误都会导致该页构建失败。
- 只支持简单的 `key: value` 行，不支持注释和嵌套结构。

## 首页模板

首页由主题中的 `layouts/home.html` 独立生成，不使用 `content/index.md`。模板必须包含 `{asset_base}` 和 `{posts}`：前者是静态资源的相对路径，后者会替换为已发布文章的 HTML 列表。文章按 `date` 字符串降序排列；日期不做格式校验，建议使用 `YYYY-MM-DD`。列表项包含文章标题、日期和相对链接，草稿不会出现在列表中。

文章页继续使用 `layouts/post.html`，需要 `{markdown}`、`{asset_base}`、`{title}` 和 `{date}` 占位符。

## 构建输出

`build`、`watch` 和 `serve` 都使用全量构建。页面和资源先写入输出目录旁的临时目录，全部成功后再替换正式输出。因此，删除或重命名文章、将文章改成草稿、删除内容或主题资源后，下一次成功构建都会清理旧产物。解析、渲染或复制失败时保留上次成功的站点；`watch` 和 `serve` 会打印错误并继续监听，构建失败不会触发浏览器刷新。

**输出目录完全由 Mizuki 管理。** 手动放入输出目录的文件也会在下一次成功构建时消失，需要发布的文件应放入内容目录或主题的 `assets` 目录。输出目录不能与内容或主题目录重叠，不能是文件系统根目录、当前工作目录或其祖先，也不能是符号链接或普通文件。

替换时先备份旧输出，再将临时目录重命名为正式输出；替换失败会尝试恢复备份。如果恢复也失败，错误信息会给出保留旧站点的备份路径。临时目录或备份清理失败时会打印警告和残留路径。

目录替换期间可能短暂无法访问输出路径。本机制不保证进程崩溃后的自动恢复，也不支持多个进程同时构建同一个输出目录。

## 回归测试

启用测试时需要 Python 3。CLI 回归测试仅使用 Python 标准库，输出事务另有 C++ 测试，所有测试均在临时目录中运行：

```sh
cmake -S . -B build -DBUILD_TESTING=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

不运行测试时，可用 `-DBUILD_TESTING=OFF` 配置，无需 Python。

