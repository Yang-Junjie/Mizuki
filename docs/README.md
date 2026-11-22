# Mizuki 文档站源码

[在线阅读](https://blog.cglab.top/Mizuki/) · [文档维护指南](./content/pages/documentation.md)

本站使用 Mizuki 自身构建：`content/pages/` 保存中文正文，`theme/` 提供 Inja 主题与本地资源，`mizuki.toml` 配置站点和导航。

在仓库根目录编译后预览：

```sh
./build/bin/mizuki serve --config docs/mizuki.toml
```

打开 <http://127.0.0.1:3939/>。源码编译见[参与贡献](./content/pages/contributing.md)，正文与导航维护、子路径预览和链接检查见[文档维护](./content/pages/documentation.md)，图片与字体来源见[主题资源说明](./theme/ASSETS.md)。
