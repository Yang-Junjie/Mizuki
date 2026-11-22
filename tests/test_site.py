"""Regressions for output planning, links, TOML configuration and independent pages."""

from html.parser import HTMLParser
import json
import subprocess
from urllib.parse import quote, unquote, urljoin, urlsplit

from support import BuilderFixture, run_tests


class Anchors(HTMLParser):
    def __init__(self, source):
        super().__init__()
        self.hrefs = []
        self.feed(source)

    def handle_starttag(self, tag, attrs):
        if tag == "a":
            self.hrefs.extend(value for key, value in attrs if key == "href")


class UrlAttributes(HTMLParser):
    def __init__(self, source):
        super().__init__()
        self.urls = []
        self.feed(source)

    def handle_starttag(self, tag, attrs):
        self.urls.extend(value for key, value in attrs if key in ("href", "src", "poster", "data"))


class SiteTests(BuilderFixture):
    def command(self, *args, success=True, cwd=None):
        result = subprocess.run(
            [str(self.binary), "build", *map(str, args)],
            cwd=cwd or self.root, capture_output=True, text=True, encoding="utf-8", timeout=15,
        )
        message = result.stdout + result.stderr
        self.assertEqual(result.returncode, 0 if success else 1, message)
        return message

    def config(self, text, path=None):
        path = path or self.root / "mizuki.toml"
        path.write_text(text, encoding="utf-8")
        return path

    def read(self, path="index.html"):
        return (self.output / path).read_text(encoding="utf-8")

    def independent(self, name="pages/about.md", body="Independent body.", extra=""):
        path = self.content / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(f"---\ntitle: About page\n{extra}---\n\n{body}\n", encoding="utf-8")

    def conflict(self, target, *sources):
        before = self.snapshot() if self.output.exists() else None
        message = self.build(success=False)
        self.assertIn("Output path conflict", message)
        self.assertIn(target, message.replace("\\", "/"))
        for source in sources:
            self.assertIn(source, message.replace("\\", "/"))
        if before is None:
            self.assertFalse(self.output.exists())
        else:
            self.assertEqual(self.snapshot(), before)
        self.assert_no_temporary_directories()

    def test_markdown_static_html_conflict_preserves_output(self):
        self.build()
        (self.content / "posts/first.html").write_text("Conflicting static HTML.")
        self.conflict("posts/first.html", "posts/first.md", "posts/first.html")

    def test_initial_conflict_leaves_no_output(self):
        (self.content / "posts/first.html").write_text("Conflict.")
        self.conflict("posts/first.html", "first.md", "first.html")

    def test_generated_homepage_conflict(self):
        (self.content / "index.html").write_text("Static homepage.")
        self.conflict("index.html", "generated homepage", "layouts/home.html", "content/index.html")

    def test_theme_content_asset_conflict(self):
        asset = self.content / "assets/css/style.css"
        asset.parent.mkdir(parents=True)
        asset.write_text("body {}")
        self.conflict("assets/css/style.css", "content/assets/css/style.css", "theme/assets/css/style.css")

    def test_file_then_directory_conflict(self):
        (self.content / "assets").write_text("A file where the assets directory belongs.")
        self.conflict("dist/assets", "content/assets", "theme/assets/css/style.css")

    def test_directory_then_file_conflict(self):
        resource = self.content / "posts/first.html/nested.txt"
        resource.parent.mkdir()
        resource.write_text("A directory where first.md would render.")
        self.conflict("posts/first.html", "first.html/nested.txt", "first.md")

    def test_case_aliases_are_rejected_on_all_platforms(self):
        (self.content / "posts/first.HTML").write_text("Case alias.")
        self.conflict("first.html", "first.md", "first.HTML")

    def test_drafts_do_not_reserve_output(self):
        self.write_page("posts/first.md", "Hidden", "Draft.", draft=True)
        (self.content / "posts/first.html").write_text("Published static HTML.")
        self.build()
        self.assertEqual(self.read("posts/first.html"), "Published static HTML.")

    def test_relative_root_reference_and_encoded_markdown_links(self):
        special = "pages/关于 space#&.md"
        self.independent(special)
        encoded = quote("../" + special, safe="/")
        body = f"""[relative](../pages/about.md?q=1&other=2#section)
[root](/pages/about.md#section)
[encoded]({encoded}?x=1&y=2#part)
[reference][about]
[self](first.md#self)
[home](/index.md)

[about]: ../pages/about.md
"""
        self.independent()
        self.write_page("posts/first.md", "Links", body)
        self.build()
        hrefs = Anchors(self.read("posts/first.html")).hrefs
        for expected in (
            "../pages/about.html?q=1&other=2#section", "/pages/about.html#section",
            quote("../pages/关于 space#&.html", safe="/") + "?x=1&y=2#part",
            "../pages/about.html", "first.html#self", "/index.html",
        ):
            self.assertIn(expected, hrefs)
        self.assertNotIn("&amp;amp;", self.read("posts/first.html"))

    def test_links_in_text_code_images_comments_and_raw_elements_are_untouched(self):
        body = '''Literal target.md and `target.md`.

[external](https://example.com/target.md?x=1&y=2)
[mail](mailto:person@example.com)
[anchor](#heading)
[static](../assets/download.zip)
![image](missing.md)

```html
<a href="missing.md">Code</a>
```

<!-- <a href="missing.md">Comment</a> -->
<script>const x = '<a href="missing.md">'; const y = '</scripture>';</script>
<style>/* <a href="missing.md"> */</style>
<textarea><a href="missing.md">Text</a></textarea>
<a-widget href="missing.md">Custom tag</a-widget>
'''
        self.write_page("posts/first.md", "Untouched", body)
        self.build()
        rendered = self.read("posts/first.html")
        for expected in (
            "Literal target.md", "<code>target.md</code>", 'src="missing.md"',
            "https://example.com/target.md?x=1&amp;y=2", "mailto:person@example.com",
            'href="#heading"', 'href="../assets/download.zip"',
            '&lt;a href=&quot;missing.md&quot;&gt;', '<!-- <a href="missing.md">',
            '<script>const x = \'<a href="missing.md">\'',
            '<a-widget href="missing.md">',
        ):
            self.assertIn(expected, rendered)

    def test_real_raw_html_anchor_links_are_rewritten(self):
        self.independent()
        self.write_page("posts/first.md", "Raw HTML", """<a data-x="href='missing.md'" HREF='../pages/about.md?a=1&amp;b=2#x'>raw</a>

<a href=../pages/about.md>unquoted</a>""")
        self.build()
        hrefs = Anchors(self.read("posts/first.html")).hrefs
        self.assertIn("../pages/about.html?a=1&b=2#x", hrefs)
        self.assertIn("../pages/about.html", hrefs)

    def test_missing_draft_and_escaping_markdown_links_fail_transactionally(self):
        self.build()
        before = self.snapshot()
        self.write_page("posts/draft.md", "Hidden", "Draft", draft=True)
        for target in ("missing.md", "draft.md", "../../../outside.md", "%2F%2Fevil.example/file.bin"):
            with self.subTest(target=target):
                self.write_page("posts/first.md", "Broken link", f"[broken]({target})")
                message = self.build(success=False)
                self.assertIn(target, message)
                self.assertEqual(self.snapshot(), before)
                self.assert_no_temporary_directories()

    def test_independent_pages_have_optional_date_and_own_layout(self):
        self.independent(body="Independent body.")
        self.write_page("pages/dated.md", "Dated page", "Also independent.")
        self.write_page("notes/deep/article.md", "Nested article", "Article body.")
        self.build()
        self.assertIn("Independent body.", self.read("pages/about.html"))
        self.assertNotIn("<time>", self.read("pages/about.html"))
        self.assertIn('href="../assets/css/style.css"', self.read("pages/about.html"))
        self.assertIn('href="../../assets/css/style.css"', self.read("notes/deep/article.html"))
        homepage = self.read()
        self.assertNotIn("About page", homepage)
        self.assertNotIn("Dated page", homepage)
        self.assertIn("Nested article", homepage)

    def test_articles_still_require_dates_and_pages_require_titles(self):
        for name, front in (
            ("notes/article.md", "title: Article\n"),
            ("pages/about.md", "date: 2026-11-16\n"),
            ("pages/about.md", "title: About\ndate: \n"),
        ):
            with self.subTest(name=name, front=front):
                path = self.content / name
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text(f"---\n{front}---\nBody.", encoding="utf-8")
                self.build(success=False)
                path.unlink()

    def test_missing_page_template_only_matters_when_page_is_published(self):
        (self.theme / "layouts/page.html").unlink()
        self.build()
        before = self.snapshot()
        self.independent()
        self.assertIn("page.html", self.build(success=False))
        self.assertEqual(self.snapshot(), before)
        self.independent(extra="draft: true\n")
        self.build()
        self.assertNotIn("pages/about.html", self.snapshot())

    def test_missing_default_configuration_uses_defaults(self):
        self.theme.rename(self.root / "theme-temp")
        (self.root / "themes").mkdir()
        (self.root / "theme-temp").rename(self.root / "themes/default")
        self.command()
        self.assertIn("Mizuki", self.read())
        self.assertIn("First post", self.read())

    def test_configuration_metadata_is_escaped_and_placeholders_not_recursive(self):
        self.config('''title = '<Site & "title"> {posts}'
description = '<Description & "quote">'
author = 'A & B'
language = 'zh-CN" data-x="value'
''')
        self.build()
        homepage = self.read()
        self.assertIn('&lt;Site &amp; &quot;title&quot;&gt; {posts}', homepage)
        self.assertIn('content="&lt;Description &amp; &quot;quote&quot;&gt;"', homepage)
        self.assertIn('content="A &amp; B"', homepage)
        self.assertIn('lang="zh-CN&quot; data-x=&quot;value"', homepage)
        self.assertNotIn('<Site', homepage)

    def test_configuration_paths_are_relative_to_config_directory(self):
        project = self.root / "settings"
        project.mkdir()
        self.content.rename(project / "文稿")
        self.theme.rename(project / "主题")
        config = self.config('content = "文稿"\ntheme = "主题"\noutput = "public/site"\n', project / "custom.toml")
        self.command("--config", config)
        self.assertTrue((project / "public/site/posts/first.html").is_file())
        self.assertFalse(self.output.exists())

    def test_cli_overrides_win_in_either_flag_order_and_resolve_from_cwd(self):
        project = self.root / "settings"
        project.mkdir()
        config = self.config('content = "missing"\ntheme = "missing"\noutput = "other"\n', project / "custom.toml")
        overrides = ("--content", "content", "--theme", "theme", "--output", "dist")
        for args in ((*overrides, "--config", config), ("--config", config, *overrides)):
            with self.subTest(args=args):
                self.command(*args)
                self.assertIn("First post", self.read())
                self.assertFalse((project / "other").exists())

    def test_invalid_configuration_preserves_successful_output(self):
        self.build()
        before = self.snapshot()
        invalid = (
            'title = 42', 'title = true', 'title = "   "', 'language = []',
            'content = ""', 'output = false', 'output = "dist\\u0000other"', 'unknown = "x"', 'navigation = "home"',
            'navigation = [42]', 'navigation = [{ title = "Home" }]',
            'navigation = [{ title = "", url = "/" }]',
            'navigation = [{ title = "Home", url = "/", typo = true }]',
            'title = "one"\ntitle = "two"', 'title = "unterminated',
        )
        for text in invalid:
            with self.subTest(config=text):
                self.config(text)
                message = self.build(success=False)
                self.assertIn("mizuki.toml", message)
                self.assertEqual(self.snapshot(), before)
                self.assert_no_temporary_directories()

    def test_explicit_missing_configuration_fails(self):
        self.assertIn("does not exist", self.command("--config", "missing.toml", success=False))
        self.assertFalse(self.output.exists())

    def test_configuration_cannot_be_inside_output(self):
        self.build()
        config = self.config('title = "Keep this config"', self.output / "config.toml")
        before = self.snapshot()
        message = self.command("--config", config, "--content", self.content,
                               "--theme", self.theme, "--output", self.output, success=False)
        self.assertIn("configuration", message.lower())
        self.assertEqual(self.snapshot(), before)

    def test_navigation_resolves_from_each_page_and_escapes_titles(self):
        self.independent()
        self.config('''[[navigation]]
title = 'Home & <start>'
url = '/'
[[navigation]]
title = "About"
url = "pages/about.md?q=1&x=2#heading"
[[navigation]]
title = "External"
url = "https://example.com/path?q=1&x=2"
''')
        self.build()
        self.assertIn("Home &amp; &lt;start&gt;", self.read())
        for page, home_url, about_url in (
            ("index.html", "index.html", "pages/about.html?q=1&x=2#heading"),
            ("posts/first.html", "../index.html", "../pages/about.html?q=1&x=2#heading"),
            ("pages/about.html", "../index.html", "about.html?q=1&x=2#heading"),
        ):
            hrefs = Anchors(self.read(page)).hrefs
            self.assertIn(home_url, hrefs)
            self.assertIn(about_url, hrefs)
            self.assertIn("https://example.com/path?q=1&x=2", hrefs)

    def test_base_url_relocates_document_links_and_resources(self):
        self.independent()
        body = '''[about](/pages/about.md?q=1&x=2#heading)
[home](/index.md)
[download](/download.bin?q=1&x=2#part)
[encoded root](%2Fdownload.bin)
[encoded page](%2Fpages/about.md)
[relative](../download.bin)
[external](https://example.net/pages/about.md)
[protocol relative](//example.net/pages/about.md)
![root image](/images/photo.png?q=1&x=2)
![relative image](../images/photo.png)
![encoded image](%2Fencoded-image.png)
![markdown resource](/image.md)
![external image](//example.net/image.png)
<video src='/movie.mp4' poster=/images/photo.png></video>
<script src="/script.js">const unchanged = '<img src="/literal.png">';</script>
<link href="/style.css" rel="stylesheet">
<a HREF=/pages/about.md>Raw link</a>
<img SRC='/images/中文%20photo.png' data-caption='/literal.png'>
<!-- <img src="/comment.png"> -->
`<img src="/code.png">`
'''
        self.write_page("posts/first.md", "Deployment", body)
        for base_url, prefix in (
            ("/", "/"), ("/blog", "/blog/"),
            ("https://example.com/blog/", "/blog/"),
            ("/中文 space/%25路径", quote("/中文 space/%路径/", safe="/")),
        ):
            with self.subTest(base_url=base_url):
                self.config(f"base_url = {json.dumps(base_url)}\n")
                self.build()
                rendered = self.read("posts/first.html")
                urls = UrlAttributes(rendered).urls
                for target in (
                    "pages/about.html?q=1&x=2#heading", "index.html", "download.bin?q=1&x=2#part",
                    "images/photo.png?q=1&x=2", "image.md", "movie.mp4", "images/photo.png",
                    "script.js", "style.css", "pages/about.html", quote("images/中文 photo.png", safe="/"),
                    "download.bin", "encoded-image.png",
                ):
                    self.assertIn(prefix + target, urls)
                for untouched in (
                    "../download.bin", "../images/photo.png", "//example.net/image.png",
                    "https://example.net/pages/about.md", "//example.net/pages/about.md",
                    "../assets/css/style.css", "../index.html",
                ):
                    self.assertIn(untouched, urls)
                self.assertIn('<img src="/literal.png">', rendered)
                self.assertIn('<!-- <img src="/comment.png"> -->', rendered)
                self.assertIn("data-caption='/literal.png'", rendered)
                self.assertNotIn("&amp;amp;", rendered)
                self.assert_no_temporary_directories()

    def test_all_generated_site_links_resolve_inside_root_and_subpath_mounts(self):
        self.independent()
        self.write_page("notes/deep/article.md", "Deep", "[About](/pages/about.md)\n![Photo](/images/中文%20photo.png)")
        image = self.content / "images/中文 photo.png"
        image.parent.mkdir()
        image.write_bytes(b"image")
        for prefix in ("/", "/blog/", quote("/中文 space/", safe="/")):
            with self.subTest(prefix=prefix):
                self.config(f'base_url = "https://example.com{prefix}"\n')
                self.build()
                for page in ("index.html", "posts/first.html", "pages/about.html", "notes/deep/article.html"):
                    for url in UrlAttributes(self.read(page)).urls:
                        resolved = urlsplit(urljoin("https://example.com" + prefix + page, url))
                        self.assertEqual(resolved.netloc, "example.com")
                        self.assertTrue(resolved.path.startswith(prefix), (page, url))
                        target = unquote(resolved.path[len(prefix):]) or "index.html"
                        self.assertTrue((self.output / target).is_file(), (page, url, target))
                self.assertFalse((self.output / "blog").exists())

    def test_base_url_template_values_are_normalized_and_optional(self):
        path = self.theme / "layouts/base.html"
        path.write_text(path.read_text(encoding="utf-8").replace("</body>",
                        '<a href="{{ site.base_path }}download.bin">Download</a>'
                        '<p>{{ site.base_url }}</p></body>'), encoding="utf-8")
        self.independent()
        for base_url, expected_url, prefix in (
            ("https://example.com", "https://example.com/", "/"),
            ("HTTP://localhost:8080/blog", "http://localhost:8080/blog/", "/blog/"),
            ("https://[::1]:8080/blog/", "https://[::1]:8080/blog/", "/blog/"),
            ("https://[::ffff:127.0.0.1]/blog/", "https://[::ffff:127.0.0.1]/blog/", "/blog/"),
            ("/中文%20space", quote("/中文 space/", safe="/"), quote("/中文 space/", safe="/")),
        ):
            with self.subTest(base_url=base_url):
                self.config(f"base_url = {json.dumps(base_url)}\n")
                self.build()
                for page in ("index.html", "posts/first.html", "pages/about.html"):
                    self.assertIn(f"<p>{expected_url}</p>", self.read(page))
                    self.assertIn(prefix + "download.bin", UrlAttributes(self.read(page)).urls)

    def test_invalid_base_urls_preserve_successful_output(self):
        self.build()
        before = self.snapshot()
        invalid = (
            "", "blog/", "//example.com/blog/", "ftp://example.com/", "https:///blog/",
            "https://user:pass@example.com/blog/", "https://example.com:0/blog/",
            "https://example.com:65536/", "https://example.com:/", "https://example.com:abc/",
            "https://[:::]/", "https://[1::2::3]/", "https://[1:2:3]/", "https://[::ffff:300.0.0.1]/",
            "https://example.com/?q=1", "/blog/#section", "/blog/../other/", "/./blog/",
            "/%2e%2e/", "/blog//nested/", "/blog%00/", "/blog%0A/", "/blog%5c/",
            "/blog\\other/", "/blog%2/", "/blog%XX/", "/blog\n/", "/C:/blog/",
        )
        for base_url in invalid:
            with self.subTest(base_url=base_url):
                self.config(f"base_url = {json.dumps(base_url)}\n")
                message = self.build(success=False)
                self.assertIn("mizuki.toml", message)
                self.assertIn("base_url", message)
                self.assertEqual(self.snapshot(), before)
                self.assert_no_temporary_directories()
        self.config("base_url = 42\n")
        self.assertIn("base_url", self.build(success=False))
        self.assertEqual(self.snapshot(), before)

    def test_cli_base_url_override_has_precedence_in_either_flag_order(self):
        self.write_page("posts/first.md", "Override", "[Home](/index.md)")
        config = self.config('base_url = "https://example.com/configured/"\n')
        overrides = ("--content", self.content, "--theme", self.theme, "--output", self.output,
                     "--base-url", "/preview")
        for args in ((*overrides, "--config", config), ("--config", config, *overrides)):
            self.command(*args)
            self.assertIn("/preview/index.html", Anchors(self.read("posts/first.html")).hrefs)
        before = self.snapshot()
        self.assertIn("base_url", self.command(*overrides[:-1], "relative/", success=False))
        self.assertEqual(self.snapshot(), before)
        self.assertIn("Missing value", self.command("--base-url", success=False))

    def test_empty_navigation_removes_nav(self):
        self.config("navigation = []")
        self.build()
        self.assertNotIn("<nav", self.read())
        self.assertNotIn("<nav", self.read("posts/first.html"))

    def test_invalid_navigation_fails_without_replacing_output(self):
        self.build()
        before = self.snapshot()
        for url in ("/missing.md", "/missing.html", "javascript:alert(1)", "../outside.html"):
            with self.subTest(url=url):
                self.config(f'[[navigation]]\ntitle = "Broken"\nurl = "{url}"\n')
                self.build(success=False)
                self.assertEqual(self.snapshot(), before)
                self.assert_no_temporary_directories()

    def test_custom_templates_use_structured_data(self):
        (self.theme / "layouts/home.html").write_text('<main>{% include "partials/posts.html" %}</main><link href="{{ asset_base }}css/style.css">' )
        (self.theme / "layouts/post.html").write_text('<h1>{{ page.title }}</h1><time>{{ page.date }}</time>{{ html(page.content) }}<link href="{{ asset_base }}css/style.css">' )
        self.config('title = "Custom site"')
        self.build()
        self.assertIn("First post", self.read())
        self.assertIn("Original body.", self.read("posts/first.html"))


if __name__ == "__main__":
    run_tests()
