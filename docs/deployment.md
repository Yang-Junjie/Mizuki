# 部署 Mizuki 站点

站点发布地址通过 `mizuki.toml` 的 `base_url` 设置，也可以使用 `--base-url` 临时覆盖。构建输出由静态文件组成，不需要在发布服务器上运行 Mizuki。

## 先在本地验证

假设站点地址是 `https://example.com/blog/`：

```toml
base_url = "https://example.com/blog/"
```

```sh
./build/bin/mizuki serve
# 打开 http://127.0.0.1:3939/blog/
./build/bin/mizuki build
```

检查首页、深层文章、独立页面、图片和下载链接，然后把 `dist/` 中的内容发布到该地址。不要再将整个输出包入 `blog/` 目录。部署到域名根目录时使用 `https://example.com/` 或 `/`。

使用完整域名构建时，也会生成 `dist/rss.xml` 和 `dist/sitemap.xml`，默认主题加入对应的 RSS 入口与页面 canonical。仅使用 `/blog/` 模拟子路径时跳过这些发布信息。配置的域名始终是正式发布域名，本地预览仍监听本机地址；在预览中点击 RSS 入口会访问配置的正式地址。

文章中仍写 `[关于](/pages/about.md)`、`![图片](/images/photo.png)`，无需手动加入 `/blog/`；相对链接也可继续使用。自定义主题使用 `{asset_base}` 引用主题资源，使用 `{base_path}` 引用其他站点资源。原样复制的 HTML、CSS 和 `srcset` 请使用适当的相对 URL。

## GitHub Pages

仓库提供 [工作流示例](../examples/github-pages.yml)，适用于包含 Mizuki 源码、`content/`、`themes/` 和 `mizuki.toml` 的站点仓库：

1. 将示例复制为 `.github/workflows/pages.yml`。示例监听 `main` 分支，其他默认分支需要修改 `branches`。
2. 在仓库 **Settings → Pages → Build and deployment** 中，将 **Source** 设为 **GitHub Actions**。
3. 提交工作流和站点内容，或手动运行 **Deploy Mizuki to Pages**。

工作流先编译 Mizuki，再通过 `actions/configure-pages` 取得实际 `base_url`，以 `--base-url` 传给构建命令，最后上传并发布 `dist/`。因此项目站点的仓库前缀、用户站点的根路径和已经配置的自定义域名都不需要在工作流里写死。生成的站点文件不包含本地预览刷新脚本。

示例声明 `contents: read`、`pages: write` 和 `id-token: write`，并使用 `github-pages` 部署环境。它保留在 `examples/` 中；复制到工作流目录并满足 Pages 设置后才会执行部署。

如果内容仓库不包含 Mizuki 源码，需要先在自己的工作流中安装或编译 Mizuki，再使用相同的构建命令和 Pages 上传、部署步骤。

工作流结构参考 GitHub 官方的 [自定义 Pages 工作流说明](https://docs.github.com/en/pages/getting-started-with-github-pages/using-custom-workflows-with-github-pages)、[静态站点示例](https://github.com/actions/starter-workflows/blob/main/pages/static.yml) 和 [`configure-pages` 输出定义](https://github.com/actions/configure-pages/blob/v5/action.yml)。
