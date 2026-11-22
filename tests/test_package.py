"""Create and exercise release archives without using the source/build tree at runtime."""

from http.client import HTTPConnection
from pathlib import Path
from urllib.parse import quote
import argparse
import hashlib
import os
import re
import socket
import subprocess
import tarfile
import tempfile
import time
import xml.etree.ElementTree as ET
import zipfile


DEPENDENCIES = ("spdlog", "md4c", "efsw", "httplib", "tomlplusplus", "json", "inja")


def check(condition, message):
    if not condition:
        raise RuntimeError(message)


def run(*args, cwd=None, timeout=30, env=None):
    result = subprocess.run(list(map(str, args)), cwd=cwd, capture_output=True,
                            text=True, encoding="utf-8", errors="replace", timeout=timeout, env=env)
    check(result.returncode == 0,
          f"Command failed: {args}\n{result.stdout}\n{result.stderr}")
    return result.stdout


def unpack(archive, destination):
    if archive.name.endswith(".tar.gz"):
        with tarfile.open(archive) as package:
            package.extractall(destination, filter="data")
    else:
        with zipfile.ZipFile(archive) as package:
            package.extractall(destination)
    roots = list(destination.iterdir())
    check(len(roots) == 1 and roots[0].is_dir(), "Archive must contain one top-level directory")
    return roots[0]


def check_runtime_dependencies(binary):
    if os.name == "nt":
        output = run("dumpbin", "/dependents", binary)
        dependencies = set(re.findall(r"^\s+(\S+[.]dll)\s*$", output, re.MULTILINE | re.IGNORECASE))
        allowed = {"kernel32.dll", "user32.dll", "advapi32.dll", "ws2_32.dll",
                   "shell32.dll", "iphlpapi.dll", "bcrypt.dll", "ntdll.dll"}
        check(dependencies and {name.lower() for name in dependencies} <= allowed,
              f"Unexpected Windows runtime dependencies:\n{output}")
    else:
        output = run("readelf", "-d", binary)
        dependencies = set(re.findall(r"Shared library: \[(.*?)\]", output))
        allowed = {"libc.so.6", "libm.so.6", "libpthread.so.0", "libdl.so.2", "librt.so.1",
                   "ld-linux-x86-64.so.2"}
        check(dependencies and dependencies <= allowed,
              f"Unexpected Linux runtime dependencies:\n{output}")
        check("(RPATH)" not in output and "(RUNPATH)" not in output,
              f"Packaged executable contains a runtime search path:\n{output}")


def preview(binary, site, environment):
    with socket.socket() as probe:
        probe.bind(("127.0.0.1", 0))
        port = probe.getsockname()[1]
    log_path = site.parent / "preview.log"
    with log_path.open("wb") as log:
        process = subprocess.Popen(
            [str(binary), "serve", "--port", str(port), "--base-url", "/blog/"],
            cwd=site, stdout=log, stderr=subprocess.STDOUT, env=environment)
        try:
            def request(path):
                connection = HTTPConnection("127.0.0.1", port, timeout=1)
                try:
                    connection.request("GET", path)
                    response = connection.getresponse()
                    return response.status, response.read()
                finally:
                    connection.close()

            def wait_for_page(path, expected):
                deadline = time.monotonic() + 15
                while time.monotonic() < deadline:
                    check(process.poll() is None,
                          f"Preview exited:\n{log_path.read_text(encoding='utf-8', errors='replace')}")
                    try:
                        status, body = request(path)
                        if status == 200 and expected.encode("utf-8") in body:
                            return
                    except (OSError, ConnectionError):
                        pass
                    time.sleep(0.05)
                raise RuntimeError(f"Preview did not serve {path}:\n"
                                   + log_path.read_text(encoding="utf-8", errors="replace"))

            wait_for_page("/blog/", "Mizuki")
            wait_for_page("/blog/posts/" + quote("第一篇文章.html"), "发布包验证")
            wait_for_page("/blog/tags/C%2B%2B/", "发布包验证")
            wait_for_page("/blog/categories/Notes/", "发布包验证")
            check(request("/blog/assets/css/style.css")[0] == 200, "Missing preview stylesheet")
            check(request("/blog/posts/draft.html")[0] == 404, "Preview published a draft")
            run(binary, "new", "posts/live.md", "--title", "Created while serving",
                cwd=site, env=environment)
            wait_for_page("/blog/posts/live.html", "Created while serving")
            wait_for_page("/blog/archives/", "Created while serving")
        finally:
            if process.poll() is None:
                process.terminate()
                try:
                    process.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.wait(timeout=5)


def smoke_test(binary, version, root, documentation_config):
    environment = os.environ.copy()
    environment.pop("LD_LIBRARY_PATH", None)
    environment.pop("LD_PRELOAD", None)
    check(run(binary, "--version", cwd=root, env=environment) == f"mizuki {version}\n",
          "Packaged executable reports the wrong version")
    site = root / "我的博客"
    run(binary, "init", site, cwd=root, env=environment)
    run(binary, "new", "posts/第一篇文章.md", "--title", '发布包验证 "中文"',
        "--description", "Downloaded binary", cwd=site, env=environment)
    run(binary, "new", "posts/draft.md", "--title", "Unpublished draft", "--draft",
        cwd=site, env=environment)
    article = site / "content/posts/第一篇文章.md"
    source = article.read_text(encoding="utf-8")
    check(source.startswith("+++\n"), "Content creation did not generate TOML front matter")
    article.write_text(source.replace("\n+++\n", '\ntags = ["C++", "中文"]\ncategories = ["Notes"]\n'
                       '[extra]\nshow_toc = false\n'
                       'cover = "images/cover.svg"\n+++\n', 1)
                       + "## Package section\n\n```cpp\nint answer = 42;\n```\n\n"
                         "$x^2$ and a note[^n].\n\n[^n]: Packaged footnote.\n", encoding="utf-8")
    cover = site / "content/images/cover.svg"
    cover.parent.mkdir()
    cover.write_text('<svg xmlns="http://www.w3.org/2000/svg" width="20" height="10">'
                     '<rect width="20" height="10" fill="teal"/></svg>', encoding="utf-8")
    run(binary, "build", "--base-url", "https://example.com/blog/", cwd=site, env=environment)
    output = site / "dist"
    for name in ("index.html", "archives/index.html", "posts/第一篇文章.html"):
        check('src="/blog/images/cover.svg"' in (output / name).read_text(encoding="utf-8"),
              f"Packaged theme did not render the document cover: {name}")
    page = (output / "posts/第一篇文章.html").read_text(encoding="utf-8")
    check('class="toc"' not in page and 'id="package-section"' in page,
          "Packaged theme did not honor the page TOC setting")
    check('<x-equation>x^2</x-equation>' in page and 'class="footnotes"' in page,
          "Packaged Markdown math or footnotes are unavailable")
    check('../assets/vendor/highlight/highlight.min.js' in page
          and '../assets/vendor/katex/katex.min.js' in page,
          "Packaged theme did not load local reading assets")
    for name in ["assets/js/code.js", "assets/js/math.js", "assets/css/reading.css",
                 "assets/vendor/highlight/LICENSE", "assets/vendor/katex/LICENSE",
                 "assets/vendor/katex/fonts/KaTeX_Main-Regular.woff2"]:
        check((output / name).is_file(), f"Missing offline reading asset: {name}")
    for name in ["index.html", "archives/index.html", "pages/about.html",
                 "posts/第一篇文章.html", "assets/css/style.css", "rss.xml", "sitemap.xml"]:
        check((output / name).is_file(), f"Missing generated file: {name}")
    for name in ["tags/index.html", "tags/C++/index.html", "tags/中文/index.html",
                 "categories/index.html", "categories/Notes/index.html"]:
        check((output / name).is_file(), f"Packaged theme did not generate taxonomy page: {name}")
    check('../tags/C%2B%2B/' in page and '../categories/Notes/' in page,
          "Packaged theme did not render the article taxonomy links")
    check(not (output / "posts/draft.html").exists(), "Build published a draft")
    feed = ET.parse(output / "rss.xml")
    check('发布包验证 "中文"' in [item.text for item in feed.findall("./channel/item/title")],
          "Created article title did not survive the package workflow")
    sitemap = ET.parse(output / "sitemap.xml")
    check(all(node.text.startswith("https://example.com/blog/")
              for node in sitemap.findall("{*}url/{*}loc")), "Incorrect sitemap deployment prefix")
    preview(binary, site, environment)
    documentation = root / "documentation"
    run(binary, "build", "--config", documentation_config, "--output", documentation,
        "--base-url", "https://example.com/Mizuki/", cwd=root, env=environment)
    for name in ["index.html", "pages/themes.html", "pages/architecture.html",
                 "pages/getting-started.html", "pages/deployment.html", "assets/css/docs.css"]:
        check((documentation / name).is_file(), f"Missing packaged documentation output: {name}")

    minimal_theme = documentation_config.parent.parent / "examples/minimal-theme"
    minimal_output = root / "minimal"
    run(binary, "build", "--theme", minimal_theme, "--output", minimal_output,
        "--base-url", "https://example.com/blog/", cwd=site, env=environment)
    minimal_page = (minimal_output / "posts/第一篇文章.html").read_text(encoding="utf-8")
    check('aria-label="独立页面"' in minimal_page and 'href="../pages/about.html"' in minimal_page,
          "Packaged minimal theme did not render the site-wide pages collection")
    check('aria-label="最近文章"' in minimal_page,
          "Packaged minimal theme did not render the site-wide posts collection")
    check((minimal_output / "tags/C++/index.html").is_file()
          and (minimal_output / "categories/Notes/index.html").is_file(),
          "Packaged minimal theme did not generate taxonomy pages")



def check_binary_package(package_root):
    executable = "mizuki.exe" if os.name == "nt" else "mizuki"
    expected = {f"bin/{executable}", "share/mizuki/LICENSE", "share/mizuki/README.md",
                "share/mizuki/CHANGELOG.md", "share/mizuki/CONTRIBUTING.md",
                "share/mizuki/licenses/fmt/LICENSE"}
    documentation_source = Path(__file__).resolve().parents[1] / "docs"
    expected.update("share/mizuki/docs/" + path.relative_to(documentation_source).as_posix()
                    for path in documentation_source.rglob("*") if path.is_file())
    examples_source = Path(__file__).resolve().parents[1] / "examples"
    expected.update("share/mizuki/examples/" + path.relative_to(examples_source).as_posix()
                    for path in examples_source.rglob("*") if path.is_file())
    expected.update(f"share/mizuki/licenses/{name}/LICENSE" + ({"md4c": ".md", "json": ".MIT"}.get(name, ""))
                    for name in DEPENDENCIES)
    actual = {path.relative_to(package_root).as_posix() for path in package_root.rglob("*")
              if path.is_file()}
    check(actual == expected, f"Unexpected package contents: {actual ^ expected}")
    for name in expected - {f"bin/{executable}"}:
        check((package_root / name).stat().st_size > 0, f"Empty package document: {name}")
    binary = package_root / "bin" / executable
    check_runtime_dependencies(binary)
    return binary


def build_source_package(source, root, version):
    check((source / "LICENSE").is_file(), "Missing project license in source archive")
    for dependency in DEPENDENCIES:
        check((source / "third_party" / dependency / "CMakeLists.txt").is_file(),
              f"Missing bundled dependency: {dependency}")
    check(not any(path.name in {".git", "__pycache__"} for path in source.rglob("*")),
          "Source archive contains local Git or Python cache files")
    check(not (source / "build").exists() and not (source / "dist").exists(),
          "Source archive contains build output")
    build = root / "offline-build"
    run("cmake", "-S", source, "-B", build, "-G", "Ninja", "-DCMAKE_BUILD_TYPE=Release",
        "-DBUILD_TESTING=OFF", "-DMIZUKI_BUILD_DISTRIBUTION=ON",
        "-DFETCHCONTENT_FULLY_DISCONNECTED=ON", "-DFETCHCONTENT_UPDATES_DISCONNECTED=ON",
        f"-DMIZUKI_RELEASE_TAG=v{version}", timeout=90)
    run("cmake", "--build", build, "--parallel", "2", timeout=420)
    binary = build / "bin" / "mizuki"
    check_runtime_dependencies(binary)
    return binary


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", required=True, type=Path)
    parser.add_argument("--version", required=True)
    parser.add_argument("--config", default="Release")
    parser.add_argument("--source", action="store_true")
    args = parser.parse_args()
    build = args.build_dir.resolve()
    config = "CPackSourceConfig.cmake" if args.source else "CPackConfig.cmake"
    run("cpack", "--config", build / config, "-C", args.config, cwd=build, timeout=90)
    platform = "windows" if os.name == "nt" else "linux"
    suffix = "source.tar.gz" if args.source else f"{platform}-x86_64" + (
        ".zip" if os.name == "nt" else ".tar.gz")
    archive = build / "packages" / f"mizuki-{args.version}-{suffix}"
    checksum = Path(str(archive) + ".sha256").read_text(encoding="utf-8").split()
    check(checksum == [hashlib.sha256(archive.read_bytes()).hexdigest(), archive.name],
          "Archive SHA-256 checksum or filename does not match")
    with tempfile.TemporaryDirectory(prefix="mizuki-package-") as temporary:
        root = Path(temporary)
        destination = root / "解压 程序"
        destination.mkdir()
        package_root = unpack(archive, destination)
        binary = (build_source_package(package_root, root, args.version) if args.source
                  else check_binary_package(package_root))
        working = root / "working directory"
        working.mkdir()
        documentation = package_root / ("docs" if args.source else "share/mizuki/docs")
        smoke_test(binary, args.version, working, documentation / "mizuki.toml")
    print(f"Verified package, checksum and site workflow: {archive.name}")


if __name__ == "__main__":
    main()
