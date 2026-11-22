# Mizuki 文档站

这个站点使用仓库里的 **Mizuki** 构建。`content/pages/` 是文档正文，`theme/` 是一个使用 Inja 的文档主题，`mizuki.toml` 提供站点配置。仓库以这个文档站作为实际示例和功能验证入口。

在仓库根目录编译并预览：

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
cmake --build build --parallel 2
./build/bin/mizuki serve --config docs/mizuki.toml
# http://127.0.0.1:3939/
```

只生成网页，或在本地检查 GitHub Pages 项目子路径：

```sh
./build/bin/mizuki build --config docs/mizuki.toml
./build/bin/mizuki serve --config docs/mizuki.toml --base-url /Mizuki/
# http://127.0.0.1:3939/Mizuki/
```

默认输出为 `build/docs/`，该目录由 Mizuki 管理，不应手动编辑。Windows 将命令中的程序路径替换为对应的 `mizuki.exe`。

- [快速开始](content/pages/getting-started.md)
- [技术写作](content/pages/writing.md)
- [主题开发](content/pages/themes.md)
- [架构设计](content/pages/architecture.md)
- [部署指南](content/pages/deployment.md)

文档正文是唯一内容源，可以在 GitHub 中直接阅读，也可以通过本站主题呈现。新增文档放在 `content/pages/`，按需补充 `mizuki.toml` 中的导航，使用相对 `.md` 链接引用其他文档。侧栏使用配置中的结构化导航；首页用 `site.pages` 自动收集所有已发布的独立页面，读取文稿自身的标题和 `description`，按输出路径升序展示卡片。新增文档无需修改首页模板；只有希望加入侧栏时才需要配置导航。个别卡片说明仍可在站点 `[extra.guide_descriptions]` 中按标题覆盖。

本站主题采用粉色马卡龙配色，首页插画与字体均随站点本地发布。配色、排版和技术写作元素的外观定制在 `theme/assets/css/docs.css`，共享阅读样式在 `theme/assets/css/reading.css`；插画和字体来源见 [主题资源说明](theme/ASSETS.md)。

想从更少代码理解主题接口，可以使用 [`examples/minimal-theme/`](../examples/minimal-theme/)，其中包含完整的标准入口与构建命令。

`.github/workflows/pages.yml` 在推送到 `main` 或手动触发时，从当前源码编译 Mizuki，构建本站并部署。Pages 的 Source 必须设为 **GitHub Actions**，私有仓库需要当前 GitHub 套餐支持 Pages。工作流用 Pages 返回的地址覆盖 `base_url`，不在主题中硬编码仓库前缀。

文档构建与链接检查已纳入 CTest：

```sh
ctest --test-dir build -R mizuki.docs --output-on-failure
```
