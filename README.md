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
- `date` 可选，按原样输出到页头，不做格式校验。
- `draft: true` 的页面会被跳过，不生成 HTML。
- 键名重复、缺少结束 `---` 或格式错误都会导致该页构建失败。
- 只支持简单的 `key: value` 行，不支持注释和嵌套结构。

`watch` 和 `serve` 每次都调用 Builder 全量构建，失败时打印错误并继续监听。输出目录应放在监听目录之外。删除或重命名源文件后，旧的 HTML 和资源需要手动清理（包括已生成过的草稿页）。

