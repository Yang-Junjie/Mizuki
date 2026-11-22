+++
title = "版本发布"
description = "维护版本号，验证二进制包与离线源码包，并通过标签生成 Release 草稿。"
+++

版本号由根目录 `CMakeLists.txt` 的 `project(Mizuki VERSION ...)` 统一声明。`mizuki --version`、压缩包名称和标签校验都使用该值；发布标签必须是 `v` 加完整版本号，例如 `v0.1.0`。

## 本地构建与验证

使用独立的构建目录，避免与日常开发构建混用：

```sh
cmake -S . -B build/distribution -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON \
  -DMIZUKI_BUILD_DISTRIBUTION=ON
cmake --build build/distribution --parallel 2
ctest --test-dir build/distribution --output-on-failure
```

需要 CMake 3.28 或更新版本、Ninja、Git、Python 3.12 或更新版本，以及 Linux GCC 或 Windows MSVC/ClangCL 工具链。Windows 在对应的开发者终端中运行，可将配置命令写在一行。分发构建目前只支持 x86_64。

`MIZUKI_BUILD_DISTRIBUTION` 禁用系统依赖查找，使用项目固定的源码依赖并静态链接；Linux 额外静态链接 libstdc++/libgcc，Windows 使用静态 MSVC 运行库。日常构建默认关闭该选项，保留原有的系统依赖与运行库选择。

测试会调用 CPack，验证通过的文件保留在 `build/distribution/packages/`：

| 产物 | 内容 |
| --- | --- |
| `mizuki-VERSION-linux-x86_64.tar.gz` | Linux 程序、使用说明、更新记录、项目与第三方许可证 |
| `mizuki-VERSION-windows-x86_64.zip` | Windows 程序和同样的文档、许可证 |
| `mizuki-VERSION-source.tar.gz` | 项目和全部依赖的源码；在 Linux 构建中生成 |
| 每个归档对应的 `.sha256` | SHA-256 摘要和归档文件名 |

二进制包只有一个顶层目录，其中包含 `bin/mizuki` 或 `bin/mizuki.exe` 和 `share/mizuki/` 文档。依赖许可证直接取自本次构建使用的源码；fmt 的许可证从 spdlog 内置 fmt 头部提取。

`mizuki.package` 检查包内文件与运行库依赖，解压到构建目录之外，再运行 `--version`、`init`、`new`、`build` 和真实的 `serve`。测试覆盖中文路径、文章标题、草稿排除、RSS/sitemap、子路径预览和服务运行中创建文章。Linux 额外运行 `mizuki.source_package`，验证源码包离线重建。

如果关闭了测试，也可以在构建完成后手动打包：

```sh
cpack --config build/distribution/CPackConfig.cmake -C Release
cpack --config build/distribution/CPackSourceConfig.cmake -C Release
```

手动打包不代表完成验证。可使用相同的验证入口，检查并重新生成产物：

```sh
python3 tests/test_package.py --build-dir build/distribution --version 0.1.0
python3 tests/test_package.py --build-dir build/distribution --version 0.1.0 --source
```

## 兼容基线

发布 CI 使用 Ubuntu 24.04 x86_64 和 Windows Server 2022 x86_64。Linux 发布包以 glibc 2.39 为最低基线，仍依赖系统的 glibc；Windows 包不依赖另行安装的 MSVC 运行库。其他系统、架构和 Windows 桌面版本尚未单独验证。

本地构建的 Linux 包使用本机工具链与 glibc，其兼容范围可能与 CI 产物不同。正式发布使用工作流验证过的产物。

## 从源码包构建

源码包保留项目源码、构建脚本、主题和测试，并将本次构建使用的依赖放在 `third_party/`；不包含 Git 数据、构建目录和 Python 缓存。CMake 自动使用这些依赖，无需再次下载：

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_TESTING=OFF -DFETCHCONTENT_FULLY_DISCONNECTED=ON
cmake --build build --parallel 2
./build/bin/mizuki --version
```

## 准备 Release 草稿

1. 修改 `CMakeLists.txt` 中的版本，并在仓库根目录的 `CHANGELOG.md` 增加相同版本的 `## VERSION` 章节。
2. 提交改动，确认分支 CI 的格式检查、功能测试和发布包验证通过。
3. 在该提交上创建并推送对应版本标签，例如 `git tag v0.1.0` 和 `git push origin v0.1.0`。
4. `release.yml` 复用完整 CI。标签与项目版本不一致时，配置阶段直接失败。
5. 两个平台通过后，工作流下载三个已验证的归档及校验文件，再次核对 SHA-256，从更新记录中提取本版本说明，并创建 Release 草稿。
6. 检查草稿中的版本、说明和文件后，在 GitHub 上手动发布。

工作流只对创建草稿的 job 授予 `contents: write`。重新运行同一标签可以更新已有草稿及其产物；已经公开的 Release 会被拒绝修改。


开发构建与日常测试见[参与贡献](contributing.md)，博客网页的托管见[部署指南](deployment.md)。
