# 文档主题资源

所有资源随文档站一起发布，页面不请求外部字体、图片或 CDN。

- `assets/images/mizuki.webp`：由用户提供的 `mizuki2.png` 转换，保留透明背景，用于首页主插画。
- `assets/images/enana.webp`：由用户提供的 `enana.png` 转换，用于首页命令示例旁的装饰。上述插画的权利归原作者所有。
- `assets/images/logo.webp`、`assets/images/favicon.png`：由用户提供的 `Pasted image.png` 生成，裁去透明留白后等比缩放并居中，保留透明背景，分别用于站点标记和浏览器图标。用户原图不作修改。
- `assets/fonts/nunito/latin.woff2`：Nunito 的 Latin 可变字体，字重 400–800，用于英文和数字；中文使用系统字体。来源为 Google Fonts 的 Nunito v32，许可见同目录 `OFL.txt`。
- `assets/fonts/jetbrains-mono/latin.woff2`：JetBrains Mono 的 Latin 可变字体，字重 400–600，用于代码。来源为 Google Fonts 的 JetBrains Mono v24，许可见同目录 `OFL.txt`。

字体通过 Google Fonts CSS API 获取；许可原文分别保存在 `google/fonts` 仓库的 `ofl/nunito/OFL.txt` 和 `ofl/jetbrainsmono/OFL.txt`。代码高亮和数学公式资源的来源见 `assets/vendor/README.txt`。

颜色、字号、字体和圆角在 `assets/css/docs.css` 中定义；技术写作元素使用 `assets/css/reading.css` 的主题变量。字体路径相对于 CSS，图片路径通过模板的 `asset_base` 生成，支持部署在仓库子路径下。
