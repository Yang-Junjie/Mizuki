# 开发约定

Mizuki 的改动应能说明解决了什么问题，并通过已有的构建、输出保护和开发预览测试。保持实现直接；只有在存在明确职责或资源生命周期时才增加类型。

## 代码结构

| 位置 | 职责 |
| --- | --- |
| `config/` | TOML 读取、配置校验，以及独立于构建流程的站点数据类型 |
| `parse/` | 解析拥有自身数据的 `Document`，将 Markdown 正文转换为 HTML |
| `render/` | 模板替换、文章列表、导航 HTML、发布链接、RSS/sitemap XML 和 HTML 链接属性扫描；只接收显式传入的数据 |
| `build/` | 规划全部输出、检查冲突、解析站内链接、协调渲染和事务发布 |
| `dev/` | 文件监听、增量触发全量构建、本地 HTTP 预览和 SSE 通知 |
| `cli/` | 参数解析、命令分发和命令级错误处理 |
| `util/` | 文件、日期、UTF-8 路径、URL 和 HTML 的小型公共操作 |

`mizuki_core` 包含配置、解析、渲染和构建。`mizuki_dev` 依赖核心库，并将 efsw 和 cpp-httplib 留在实现文件中。可执行文件负责 CLI，C++ 测试链接同一份核心库。第三方依赖不使用项目自身的警告和格式化选项。

## 接口与失败处理

- 一次操作直接返回结果。`parse_document` 返回 `Document`，`render_markdown` 和 `Template::render` 返回字符串，`build_site` 返回发布后的统计。避免要求调用方按顺序调用 `load`、`render`、`get` 来读取隐藏状态。
- `Document` 拥有元数据和正文；`Template` 在创建时读取模板，每次渲染使用局部结果。`TemplateValue` 是渲染调用期间的视图，底层字符串必须存活到调用结束。文本与 HTML、必需与可选占位符使用命名工厂和枚举表示。
- 失败抛出带上下文的异常。文件或模板边界补充路径，CLI 转为退出码；持续监听在每次构建边界记录错误并等待下一次变化。避免吞掉底层原因或同时维护成功标记、错误字段和结果字段。
- 资源由 RAII 对象管理。`OutputTransaction` 拥有临时输出，在全部成功后发布，失败时保留原站点。文件监听对象负责停止并释放自己的监听。
- 监听配置与成功发布配置分别保存。有效的新输入目录在构建前开始监听；构建成功后才更新预览根目录并通知浏览器。HTTP 请求通过同一次快照取得目录、部署路径和版本。SSE 传递新版本和部署路径，使浏览器在路径切换后保留当前页面位置。

模板替换保持单次扫描：插入的正文、标题或导航不再参与占位符替换。路径转换与 URL 编解码是不同操作，分别使用 `util/path` 和 `util/url`。`BaseUrl` 保存域名部分和经过 URL 编码、以 `/` 开始并结尾的部署路径；内容根路径从站点根解析，输出目录不包含部署前缀。

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

## 测试

Python 测试只依赖标准库，共享夹具在 `tests/support.py`，导入测试模块不会读取或修改命令行参数。可运行单组测试，也可通过环境变量指定程序后使用 unittest discovery：

```sh
python3 tests/test_site.py build/bin/mizuki
MIZUKI_BINARY="$PWD/build/bin/mizuki" python3 -m unittest discover -s tests -v
```

Windows 上使用对应的 `.exe` 路径并按终端语法设置环境变量。C++ 测试验证输出事务回滚、文档所有权、模板复用和错误上下文；Python 测试覆盖完整构建、发布信息和真实预览进程。RSS 和 sitemap 使用标准库 XML 解析器验证实际输出。

涉及输出或失败路径的修改，应验证旧产物是否保留；涉及热重载的修改，应验证修复后能否恢复，以及失败是否错误触发刷新。修改生成逻辑时，应比较文件清单和实际内容，避免只检查进程退出码。

## 发布信息

日期在文档解析时按有效的 `YYYY-MM-DD` 日历日期校验，发布计划只包含已发布页面。构建计划统一整理文章顺序；首页分页、年份归档和 RSS 使用同一份文章摘要。分页切片与输出路径也在规划阶段确定，RSS 不使用首页切片。绝对地址经 `published_url` 统一拼接域名、已编码部署路径与经过编码的输出路径，避免重复编码。

RSS/sitemap 渲染保持确定性，不使用本机时区、语言环境、构建时间或文件修改时间。只有配置完整域名时才预留并生成 XML；同名冲突在输出规划阶段报告。RSS 的单行纯文本摘要先进行 HTML 转义，再进行 XML 转义，避免订阅阅读器将其当成 HTML 标记。XML 文本需要转义并拒绝无效 UTF-8 或 XML 字符，失败通过同一输出事务回滚。自定义主题的新增占位符均为可选，所有模板值在同步渲染完成前保持有效。

## 文章列表

`paginate_by` 和 `archives` 默认关闭，使旧配置与主题保留原有行为。构建计划先收集已发布文章，再按日期降序、输出 URL 升序排列，保存每张首页的切片与输出路径。空列表也保留一张首页。归档在同一顺序上按年份分组，不重新排序。

所有生成页面进入同一输出注册表，不能绕过文件/目录和大小写冲突检查。列表渲染接收当前输出路径，用于生成相对文章链接和资源路径；分页链接使用目录形式，第一页始终指向站点根目录。目录列表的 canonical 与 sitemap 使用结尾 `/`，内容页面维持自身输出地址。

模板缺失与渲染失败通过输出事务回滚。实际多页时要求首页模板含 `{pagination}`，启用归档时要求归档模板含 `{archives}` 与 `{asset_base}`。测试应检查排序与切片、空站点、整页边界、同日文章、RSS 全量保留、编码后的子路径链接、旧分页清理，以及真实预览中的失败版本保留与恢复。
