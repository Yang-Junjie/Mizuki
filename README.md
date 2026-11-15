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

两个命令都支持指定目录，相对路径以执行命令时所在的目录为准：

```sh
./build/bin/mizuki watch --content content --theme themes/default --output dist
```

`watch` 每次都调用 Builder 全量构建，失败时打印错误并继续监听。输出目录应放在监听目录之外。删除或重命名源文件后，旧的 HTML 和资源需要手动清理。
