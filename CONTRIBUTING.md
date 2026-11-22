# 开发约定

Mizuki 的改动应能说明解决了什么问题，并通过已有的构建、输出保护和开发预览测试。保持实现直接；只有在存在明确职责或资源生命周期时才增加类型。

架构与数据流见[架构设计](docs/content/pages/architecture.md)。文档站使用 Mizuki 自身构建，配置、主题和预览方法见 [docs/README.md](docs/README.md)。修改对外行为时同步更新 `docs/content/pages/` 中的唯一正文来源。

仓库以 `docs/` 文档站作为实际示例和功能验证入口，构建或预览时显式指定 `--config docs/mizuki.toml`。博客场景和默认主题由自动化测试创建临时站点验证，无需在仓库根目录维护另一份示例内容。

## 代码结构

| 位置 | 职责 |
| --- | --- |
| `config/` | TOML 读取、配置校验，以及独立于构建流程的站点数据类型 |
| `parse/` | 解析拥有自身数据的 `Document`，将 Markdown 正文转换为拥有自身 HTML 和标题信息的 `RenderedMarkdown` |
| `render/` | Inja 主题环境、模板缓存、发布 URL、RSS/sitemap XML 和 HTML 链接属性扫描 |
| `build/` | 规划全部输出、检查冲突、解析站内链接、构造模板数据、协调渲染和事务发布 |
| `dev/` | 文件监听、增量触发全量构建、本地 HTTP 预览和 SSE 通知 |
| `scaffold/` | 创建独立站点与 Markdown 文稿，管理只创建、不覆盖的文件操作 |
| `cli/` | 参数解析、命令分发和命令级错误处理 |
| `util/` | 文件、日期、UTF-8 路径、URL 和 HTML 的小型公共操作 |

`mizuki_core` 包含配置、解析、渲染、构建与站点/文稿创建。`mizuki_dev` 依赖核心库，并将 efsw 和 cpp-httplib 留在实现文件中。可执行文件负责 CLI，C++ 测试链接同一份核心库。第三方依赖不使用项目自身的警告和格式化选项。

## 接口与失败处理

- 一次操作直接返回结果。`parse_document` 返回 `Document`，`render_markdown` 返回拥有 HTML 与标题列表的 `RenderedMarkdown`，`Theme::render` 返回字符串，`build_site` 返回发布后的统计。避免要求调用方按顺序调用 `load`、`render`、`get` 来读取隐藏状态。
- `Document` 拥有元数据和正文；模板上下文使用拥有数据的 JSON 值。每次构建创建一个 `Theme`，按需读取并缓存布局、组件与父模板，各页渲染使用局部结果。Inja 留在实现文件中，JSON 是配置与模板数据的公共值类型。
- 失败抛出带上下文的异常。文件或模板边界补充路径，CLI 转为退出码；持续监听在每次构建边界记录错误并等待下一次变化。避免吞掉底层原因或同时维护成功标记、错误字段和结果字段。
- 资源由 RAII 对象管理。`OutputTransaction` 拥有临时输出，在全部成功后发布，失败时保留原站点。文件监听对象负责停止并释放自己的监听。
- 监听配置与成功发布配置分别保存。有效的新输入目录在构建前开始监听；构建成功后才更新预览根目录并通知浏览器。HTTP 请求通过同一次快照取得目录、部署路径和版本。SSE 传递新版本和部署路径，使浏览器在路径切换后保留当前页面位置。

主题统一使用 Inja，不提供旧占位符兼容层。字符串表达式自动转义，正文使用独立输出函数 `html()`；数据中的模板标记不参与再次解析。模板文件只能从主题 `layouts/` 内加载；加载栈检查循环与深度。`config/values` 将 TOML 扩展参数转为保留类型的 JSON，表递归合并，数组和标量整体覆盖。路径转换与 URL 编解码是不同操作，分别使用 `util/path` 和 `util/url`。`BaseUrl` 保存域名部分和经过 URL 编码、以 `/` 开始并结尾的部署路径；内容根路径从站点根解析，输出目录不包含部署前缀。

## 格式与构建

C++ 使用仓库的 `.clang-format`，以 **clang-format 18** 为准；`.editorconfig` 约定编码、缩进和行尾。CMake 优先查找 `clang-format-18`，然后查找 `clang-format`，也可通过 `-DMIZUKI_CLANG_FORMAT=/absolute/path/to/clang-format` 指定程序。

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
cmake --build build --target format
cmake --build build --target check-format
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
```

未安装 clang-format 时不创建格式化目标，正常编译和测试不受影响。CI 使用 clang-format 18 检查格式，并在 Linux 和 Windows 上构建、运行测试。

项目版本只在根目录 `CMakeLists.txt` 的 `project(... VERSION ...)` 中声明，CLI 版本头和压缩包名称从这里生成。发布构建使用 `MIZUKI_BUILD_DISTRIBUTION=ON`，固定源码依赖、静态链接第三方库和 C++ 运行库。打包、兼容基线与标签流程见 [发布说明](RELEASING.md)。

## 测试

Python 测试只依赖标准库，共享夹具在 `tests/support.py`，导入测试模块不会读取或修改命令行参数。可运行单组测试，也可通过环境变量指定程序后使用 unittest discovery：

```sh
python3 tests/test_site.py build/bin/mizuki
MIZUKI_BINARY="$PWD/build/bin/mizuki" python3 -m unittest discover -s tests -v
```

Windows 上使用对应的 `.exe` 路径并按终端语法设置环境变量。C++ 测试验证输出事务回滚、文档所有权、模板复用和错误上下文；Python 测试覆盖完整构建、发布信息和真实预览进程。RSS 和 sitemap 使用标准库 XML 解析器验证实际输出。

发布构建的测试需要 Python 3.12 或更新版本。`mizuki.package` 生成归档和校验文件，在临时中文路径下解压，检查实际文件清单、许可证和运行库依赖，再使用包内程序完成创建、构建、预览与监听重建。Linux 的 `mizuki.source_package` 还会生成包含依赖的源码包，在关闭 FetchContent 下载的情况下重新构建并验证站点流程。两个测试串行使用打包目录，验证通过的归档保留在构建目录的 `packages/` 中，供 CI 上传。

涉及输出或失败路径的修改，应验证旧产物是否保留；涉及热重载的修改，应验证修复后能否恢复，以及失败是否错误触发刷新。修改生成逻辑时，应比较文件清单和实际内容，避免只检查进程退出码。

## 发布信息

日期在文档解析时按有效的 `YYYY-MM-DD` 日历日期校验，发布计划只包含已发布页面。构建计划统一整理文章顺序；首页分页、年份归档和 RSS 使用同一份文章摘要。分页切片与输出路径也在规划阶段确定，RSS 不使用首页切片。绝对地址经 `published_url` 统一拼接域名、已编码部署路径与经过编码的输出路径，避免重复编码。

RSS/sitemap 渲染保持确定性，不使用本机时区、语言环境、构建时间或文件修改时间。只有配置完整域名时才预留并生成 XML；同名冲突在输出规划阶段报告。RSS 的单行纯文本摘要先进行 HTML 转义，再进行 XML 转义，避免订阅阅读器将其当成 HTML 标记。XML 文本需要转义并拒绝无效 UTF-8 或 XML 字符，失败通过同一输出事务回滚。模板上下文由 `build/context` 构造，发布 URL 与 RSS/sitemap 使用同一套编码规则。

## 创建站点与文稿

默认主题保留在 `themes/default/`；CMake 根据这组文件生成构建目录中的 `embedded_theme.hpp`，不维护第二份手写主题。文件增删或内容变化都会触发重新生成。`init` 从嵌入资源输出可编辑的主题，不通过当前目录、源码绝对路径或可执行文件旁的资源目录寻找模板。`mizuki` 安装目标使用独立的 `mizuki` component。

`FileCreation` 使用 C++23 的排他文件创建模式，不截断已有文件；提交前失败时按逆序移除自己创建的路径，只删除空目录，不递归删除其他内容。调用方先验证参数、目标目录和文稿内容，再开始文件操作。`new` 复用配置路径规则和文档解析器；本地日期只用于新文件创建，不进入构建、RSS 或 sitemap 的生成过程。

新增测试需要验证实际生成的站点可以构建、默认主题与源文件一致、中文和引号内容保真、草稿与独立页面的行为，以及重复创建和中途失败对已有文件的保护。真实预览测试覆盖初始化后启动服务和服务运行中创建文章。

## 文章列表

`paginate_by` 和 `archives` 默认关闭。构建计划先收集已发布文章，再按日期降序、输出 URL 升序排列，保存每张首页的切片与输出路径。空列表也保留一张首页。归档在同一顺序上按年份分组，不重新排序。

所有生成页面进入同一输出注册表，不能绕过文件/目录和大小写冲突检查。列表渲染接收当前输出路径，用于生成相对文章链接和资源路径；分页链接使用目录形式，第一页始终指向站点根目录。目录列表的 canonical 与 sitemap 使用结尾 `/`，内容页面维持自身输出地址。

模板缺失与渲染失败通过输出事务回滚。主题通过结构化 `posts`、`archives` 与 `pagination` 控制标记，不检查固定变量是否出现；只要求实际使用的模板存在。测试应检查排序与切片、空站点、整页边界、同日文章、RSS 全量保留、编码后的子路径链接、旧分页清理，以及真实预览中的失败版本保留与恢复。

## 正文标题与目录

Markdown 沿用 md4c 渲染，再从实际生成的 HTML 中收集标题、分配 ID 并加入章节链接。正文 HTML 与标题列表在同一次处理后返回；构建层随后改写站内 URL，并将同一份标题转换为 `page.headings` 和 `toc` 数据。目录的具体 HTML 由主题组件负责。默认主题是 `init` 内嵌主题的唯一来源。

新增 ID 按正文顺序分配，并避开所有已存在的正文 ID。只转换 ASCII 大小写，空白处理不依赖本机 locale；HTML 实体复用 md4c 的转换结果，不维护另一份完整实体表。HTML 扫描要保留属性、注释和原始文本，代码中的伪标题不能进入目录。一级标题会切断二级标题的归属；孤立三级标题生成顶层条目，避免无效的列表嵌套。

`tests/test_reading.py` 检查实际输出中的 ID、章节链接、目录文字与列表层级，并验证跨文章链接、部署前缀、重复构建和不显示目录的自定义主题。初始化测试同时检查内嵌主题与默认主题一致，以及示例文章的章节跳转。

## 主题开发

主题接口与迁移规则见 [主题开发指南](docs/content/pages/themes.md)，最小可用主题位于 [`examples/minimal-theme/`](examples/minimal-theme/)。全站 `site.posts`、`site.pages` 与当前列表 `posts`、`archives` 的范围应保持明确，所有摘要通过 `summary_data()` 按当前输出位置转换，均不携带正文。新增主题能力应通过 `tests/test_theme.py` 检查完整构建输出：同一数据能否生成不同布局、配置合并是否保留类型、HTML 是否正确转义、布局和组件路径是否被限制在主题目录内。错误路径应检查旧输出保留与修复后的恢复。`tests/test_theme_data.py` 检查各入口的全站集合、排序、草稿、摘要类型、深层链接与最小主题，文档测试验证新增文稿自动进入首页。预览测试覆盖组件、主题默认参数和站点覆盖参数的热重载，发行测试覆盖 Inja 与 JSON 的依赖源码及许可证。

## 技术写作与本地资源

`MarkdownOptions` 定义 `[markdown]` 的语法开关；HTML 分析收集正文能力与语言信息，构建层将它们交给主题。浏览器脚本、资源引用与样式属于主题。`tests/test_markdown_features.py` 检查语法开关、实际 HTML、资源路径与静态回退，预览测试检查配置热重载。

默认主题和文档主题共享阅读脚本、样式和组件的约定，两份文件需要同步修改；回归测试会检查一致性。`python3 tools/update_reading_assets.py` 下载固定版本的 highlight.js 与 KaTeX，校验包摘要，并更新两份 vendor 资源、所有被引用的 WOFF2 字体及许可证。更新版本时同时更新固定摘要与版本说明。构建与初始化不执行下载脚本，不要求 npm。

可选浏览器检查使用 `playwright-core` 和本机 Chromium，验证两种主题的浅色/深色、手机/桌面、复制原文、公式与 MathML、字体本地加载，以及禁用 JavaScript 后的脚注链接。将 `playwright-core` 安装到项目外，再运行：

```sh
npm install --prefix /tmp/mizuki-browser --no-audit --no-fund playwright-core
NODE_PATH=/tmp/mizuki-browser/node_modules node tests/browser_reading.cjs build/bin/mizuki
```

默认浏览器路径为 `/usr/bin/google-chrome-stable`，可用 `MIZUKI_CHROME` 指定其他 Chromium 可执行文件。测试使用临时站点，并阻止所有站点以外的网络请求；不属于默认 CTest 的依赖。
