+++
title = "技术写作"
description = "编写有语言高亮的代码、数学公式、脚注和提示块，并通过主题调整呈现。"
+++

Mizuki 用 MD4C 解析 Markdown，生成的正文可以直接阅读。默认主题与本站主题再用本地 highlight.js 和 KaTeX 增强代码与公式；脚注和提示块不依赖 JavaScript。

## 配置 Markdown

站点的 `mizuki.toml` 控制语法：

```toml
[markdown]
math = true
footnotes = true
admonitions = true
```

`math` 默认关闭；`footnotes` 与 `admonitions` 默认开启。`mizuki init` 创建的站点显式开启三项。表内只接受这些布尔字段，拼错键名或类型会报错。配置修改会触发开发预览重建。

语法由站点决定，配色、交互和资源引用由主题决定。关闭主题的公式渲染不会关闭数学语法，而会显示 TeX 原文。

## 代码与语言

围栏后标注语言：

````markdown
```cpp
#include <iostream>

int main() {
    std::cout << "Hello, Mizuki!\n";
}
```
````

实际效果：

```cpp
#include <iostream>

int main() {
    std::cout << "Hello, Mizuki!\n";
}
```

默认主题提供 C/C++、Python、Bash/Shell、JSON、TOML/INI、HTML/XML、CSS、JavaScript、TypeScript、CMake、Rust、Go 等语言，采用 highlight.js 的 common 集合加 CMake。`cpp`、`c++`、`js`、`html` 等别名可直接使用。

未标注语言或不支持的语言保留普通代码，不猜测语言。行内代码不高亮；代码块保留空格、换行和水平滚动。复制按钮复制原始文本，不带语言名称或按钮文字，需要浏览器提供 Clipboard API（通常是 HTTPS 或 localhost）。关闭 JavaScript 后仍然显示代码。

```toml
[extra]
code_highlight = true
code_copy = true
math_render = true
```

这三个参数是默认主题与文档主题的约定。可以独立关闭高亮或复制；行号和行强调暂未提供。

## 行内公式与独立公式

开启 `math` 后，`$...$` 表示行内公式，例如 $e^{i\pi}+1=0$。

使用独占行的 `$$` 编写独立公式：

```text
$$
\sum_{n=1}^{\infty} \frac{1}{n^2} = \frac{\pi^2}{6}
$$
```

$$
\sum_{n=1}^{\infty} \frac{1}{n^2} = \frac{\pi^2}{6}
$$

分式、矩阵和对齐等使用 KaTeX 支持的 TeX 语法。公式只在 MD4C 标记的数学节点中渲染，不扫描页面正文，代码块和行内代码中的美元符号保持原样。

启用数学语法后，金额建议写成 `\$5` 和 `\$10`，呈现为 \$5 和 \$10，防止两个美元符号被当成公式边界。关闭数学语法时美元符号按普通 Markdown 处理。

默认主题输出 HTML 与 MathML；宽公式在自身区域水平滚动。JavaScript 关闭或主题关闭 `math_render` 时保留 TeX 原文。无效公式也保留原文，并通过浏览器控制台报告原因。公式不允许启用 KaTeX 的可信 HTML 命令。

标题也可以使用数学语法；目录名称与章节 ID 按源 TeX 生成，浏览器排版不会改变已生成的链接。

## 脚注与多次引用

脚注使用引用和定义：

```markdown
正文中的补充说明[^source]，也可以再次引用[^source]。

[^source]: 支持 **强调**、代码和链接的脚注正文。
```

这里是第一次引用[^source]，这里再次引用同一脚注[^source]。

[^source]: 脚注由构建阶段生成。**强调**、`code` 和[主题文档](themes.md)都可以使用，每个引用位置都有返回链接。

编号、脚注区域和返回链接均为静态 HTML。禁用 JavaScript 不影响跳转。正文中引用不存在的脚注按普通文字处理。

## 提示块

支持 `NOTE`、`TIP`、`IMPORTANT`、`WARNING`、`CAUTION` 五种类型：

```markdown
> [!NOTE]
> 一段需要单独说明的内容。
```

> [!NOTE]
> 构建器提供语义化标记，主题控制边框、标题与颜色。

> [!TIP]
> 用 `mizuki serve` 同时预览浅色、深色和手机宽度下的效果。

> [!IMPORTANT]
> 主题的脚本、字体和样式都随站点发布，无需运行时 CDN。

> [!WARNING]
> 启用数学语法后，请转义金额中的美元符号。

> [!CAUTION]
> 在重写主题样式后，检查长代码和宽公式是否仍可横向滚动。

## 自定义主题样式

默认主题将阅读样式放在 `assets/css/reading.css`，通过 `partials/reading.html` 按页引用。可以直接编辑，也可以在其后加载自己的 CSS：

```html
{% block head %}
<link rel="stylesheet" href="{{ asset_base }}css/custom.css">
{% endblock %}
```

上面的 `head` 扩展点由默认主题提供。文档主题也提供相同扩展点。覆盖示例：

```css
:root {
  --syntax-keyword: #a31542;
  --syntax-string: #0a526b;
  --code-font-size: 0.95rem;
  --code-line-height: 1.8;
  --code-radius: 4px;
  --math-font-size: 1.15em;
  --footnote-font-size: 0.85em;
  --admonition-note: #0969da;
}

@media (prefers-color-scheme: dark) {
  :root {
    --syntax-keyword: #ff8fa3;
    --syntax-string: #a5d6ff;
  }
}
```

完整变量、HTML 约定和自行替换渲染库的方法见[主题开发](themes.md#正文能力与资源)。
