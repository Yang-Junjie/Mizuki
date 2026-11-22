"""Site-wide summaries, page-local collections and a buildable theme example."""

from html.parser import HTMLParser
from urllib.parse import quote, unquote, urljoin
import shutil

from support import BuilderFixture, PROJECT_ROOT, run_tests


class ThemePage(HTMLParser):
    def __init__(self, source):
        super().__init__(convert_charrefs=True)
        self.collections = {}
        self.current = None
        self.page = {}
        self.links = []
        self.feed(source)

    def handle_starttag(self, tag, attributes):
        attributes = dict(attributes)
        if tag == "main" and "data-kind" in attributes:
            self.page = attributes
        if tag == "section" and "data-collection" in attributes:
            self.current = attributes["data-collection"]
        if tag == "aside" and attributes.get("aria-label") == "最近文章":
            self.current = "recent"
        if tag == "nav" and attributes.get("aria-label") == "独立页面":
            self.current = "page-navigation"
        if self.current is not None:
            self.collections.setdefault(self.current, [])
        for name in ("href", "src"):
            if name in attributes:
                self.links.append(attributes[name])
        if tag == "a" and self.current is not None:
            self.collections[self.current].append(attributes)

    def handle_endtag(self, tag):
        if tag in ("section", "aside", "nav"):
            self.current = None


class ThemeDataTests(BuilderFixture):
    def config(self, text):
        (self.root / "mizuki.toml").write_text(text, encoding="utf-8")

    def read(self, name="index.html"):
        return (self.output / name).read_text(encoding="utf-8")

    def document(self, name, metadata, body="Body kept in the current page only."):
        target = self.content / name
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_text(f"+++\n{metadata}\n+++\n\n{body}\n", encoding="utf-8")

    def probe_theme(self):
        card = '''{% for item in summaries %}<a href="{{ item.url }}"
 data-title="{{ item.title }}" data-date="{{ item.date }}"
 data-description="{{ item.description }}" data-fields="{{ length(item) }}"
 data-has-content="{{ existsIn(item, "content") }}"
 data-extra-object="{{ isObject(item.extra) }}"
 data-has-site-only="{{ existsIn(item.extra, "site_only") }}"
 data-count="{{ default(item.extra.count, 0) + 1 }}"
 data-integer="{{ isInteger(default(item.extra.count, 0)) }}"
 data-enabled="{{ default(item.extra.enabled, false) }}"
 data-boolean="{{ isBoolean(default(item.extra.enabled, false)) }}"
 data-ratio="{{ default(item.extra.ratio, 0) + 0.25 }}"
 data-author="{{ default(item.extra.author.name, "") }}"
 data-tags="{{ join(default(item.extra.tags, []), "|") }}">{{ item.title }}</a>{% endfor %}'''
        (self.theme / "layouts/partials/summary-probe.html").write_text(card, encoding="utf-8")
        layout = '''<main data-kind="{{ page.kind }}" data-title="{{ page.title }}"
 data-number="{{ pagination.current_page }}" data-total="{{ pagination.total_pages }}"
 data-author="{{ default(page.extra.author.name, "") }}">{{ html(page.content) }}</main>
<section data-collection="site-posts">{% set summaries = site.posts %}{% include "partials/summary-probe.html" %}</section>
<section data-collection="site-pages">{% set summaries = site.pages %}{% include "partials/summary-probe.html" %}</section>
<section data-collection="posts">{% set summaries = posts %}{% include "partials/summary-probe.html" %}</section>
<section data-collection="archives">{% for group in archives %}{% set summaries = group.posts %}{% include "partials/summary-probe.html" %}{% endfor %}</section>'''
        for name in ("home.html", "post.html", "page.html", "archives.html", "special.html"):
            (self.theme / "layouts" / name).write_text(layout, encoding="utf-8")

    def populate(self):
        self.document("posts/first.md", '''title = '<First & "post">'
date = "2026-11-20"
description = "Own summary <&>"
[extra]
count = 7
enabled = true
ratio = 1.5
tags = ["C++", "themes"]
[extra.author]
name = "Post author <&>"''')
        self.write_page("posts/earlier.md", "Earlier", "Earlier body.", date="2025-01-01")
        self.write_page("notes/中文 space.md", "Same date", "Nested body.", date="2026-11-20")
        self.document("pages/deep/关于 space.md", '''title = "About"
layout = "special.html"
[extra]
count = 11
enabled = false
ratio = 2.5
tags = ["independent", "page"]
[extra.author]
name = "Page author"''')
        self.document("pages/a.md", 'title = "Z title"\ndate = "2020-01-01"')
        self.document("pages/z.md", 'title = "A title"\ndate = "2026-11-22"')
        self.write_page("posts/draft.md", "Hidden post", "Draft body.", draft=True)
        self.document("pages/draft.md", 'title = "Hidden page"\ndraft = true')
        (self.content / "copied.html").write_text("An asset, not a document.", encoding="utf-8")
        return (["notes/中文 space.html", "posts/first.html", "posts/earlier.html"],
                ["pages/a.html", "pages/deep/关于 space.html", "pages/z.html"])

    def assert_links(self, entries, expected, base_url, name):
        self.assertEqual([urljoin(base_url + name, entry["href"]) for entry in entries],
                         [base_url + quote(path, safe="/") for path in expected])
        for path in expected:
            self.assertTrue((self.output / path).is_file(), path)

    def test_site_collections_are_complete_in_every_layout_and_urls_follow_the_current_page(self):
        self.probe_theme()
        articles, pages = self.populate()
        for base_url in ("https://example.test/", "https://example.test/blog/",
                         "https://example.test/%E6%96%87%E6%A1%A3%20%E7%AB%99/"):
            with self.subTest(base_url=base_url):
                self.config(f'base_url = "{base_url}"\npaginate_by = 1\narchives = true\n'
                            '[extra]\nsite_only = "Site parameter"\n')
                self.build()
                outputs = ["index.html", "page/2/index.html", "page/3/index.html",
                           "archives/index.html", *articles, *pages]
                for name in outputs:
                    rendered = ThemePage(self.read(name))
                    for collection, expected in (("site-posts", articles), ("site-pages", pages)):
                        entries = rendered.collections[collection]
                        self.assert_links(entries, expected, base_url, name)
                        for entry in entries:
                            self.assertEqual(entry["data-fields"], "5")
                            self.assertEqual(entry["data-has-content"], "false")
                            self.assertEqual(entry["data-extra-object"], "true")
                            self.assertEqual(entry["data-has-site-only"], "false")
                    if rendered.page["data-kind"] == "home":
                        number = int(rendered.page["data-number"])
                        self.assertEqual(rendered.page["data-total"], "3")
                        self.assert_links(rendered.collections["posts"],
                                          articles[number - 1:number], base_url, name)
                    else:
                        self.assertEqual(rendered.collections["posts"], [])
                        self.assertEqual(rendered.page["data-number"], "1")
                        self.assertEqual(rendered.page["data-total"], "1")
                    self.assert_links(rendered.collections["archives"],
                                      articles if name == "archives/index.html" else [], base_url, name)
                # The same input produces exactly the same complete output, including site data.
                before = self.snapshot()
                self.build()
                self.assertEqual(self.snapshot(), before)

    def test_summary_metadata_preserves_types_and_page_data_stays_separate(self):
        self.probe_theme()
        self.populate()
        self.config('description = "Site description"\npaginate_by = 1\narchives = true\n')
        self.build()
        for name in ("index.html", "posts/first.html", "pages/deep/关于 space.html",
                     "page/3/index.html", "archives/index.html"):
            with self.subTest(name=name):
                rendered = ThemePage(self.read(name))
                post = rendered.collections["site-posts"][1]
                page = rendered.collections["site-pages"][1]
                self.assertEqual(post["data-title"], '<First & "post">')
                self.assertEqual(post["data-description"], "Own summary <&>")
                self.assertEqual(post["data-count"], "8")
                self.assertEqual(post["data-integer"], "true")
                self.assertEqual(post["data-enabled"], "true")
                self.assertEqual(post["data-boolean"], "true")
                self.assertEqual(post["data-ratio"], "1.75")
                self.assertEqual(post["data-author"], "Post author <&>")
                self.assertEqual(post["data-tags"], "C++|themes")
                self.assertEqual(page["data-date"], "")
                self.assertEqual(page["data-description"], "")
                self.assertEqual(page["data-count"], "12")
                self.assertEqual(page["data-enabled"], "false")
                self.assertEqual(page["data-boolean"], "true")
                self.assertEqual(page["data-ratio"], "2.75")
                self.assertEqual(page["data-author"], "Page author")
                self.assertEqual(page["data-tags"], "independent|page")
                author = {"posts/first.html": "Post author <&>",
                          "pages/deep/关于 space.html": "Page author"}.get(name, "")
                self.assertEqual(rendered.page["data-author"], author)
        self.assertIn('data-title="&lt;First &amp; &quot;post&quot;&gt;"', self.read())
        self.assertIn("Body kept in the current page only.", self.read("posts/first.html"))
        self.assertNotIn("Body kept in the current page only.", self.read())

    def test_collections_always_exist_when_either_or_both_content_kinds_are_empty(self):
        self.probe_theme()
        self.config('archives = true\n')
        for article_count, page_count in ((0, 0), (1, 0), (0, 1)):
            with self.subTest(articles=article_count, pages=page_count):
                shutil.rmtree(self.content)
                self.content.mkdir()
                self.write_page("posts/draft.md", "Hidden post", "Draft.", draft=True)
                self.document("pages/draft.md", 'title = "Hidden page"\ndraft = true')
                if article_count:
                    self.write_page("posts/only.md", "Only post", "Post.")
                if page_count:
                    self.document("pages/only.md", 'title = "Only page"')
                self.build()
                for path in self.output.rglob("*.html"):
                    page = ThemePage(path.read_text(encoding="utf-8"))
                    self.assertEqual(len(page.collections["site-posts"]), article_count)
                    self.assertEqual(len(page.collections["site-pages"]), page_count)

    def test_content_edits_renames_and_drafts_update_every_page_collection(self):
        self.probe_theme()
        self.document("pages/about.md", 'title = "Before"')
        self.build()
        (self.content / "pages/about.md").rename(self.content / "pages/renamed.md")
        self.document("pages/renamed.md", 'title = "After"')
        self.write_page("posts/first.md", "Now a draft", "Draft.", draft=True)
        self.build()
        self.assertFalse((self.output / "pages/about.html").exists())
        self.assertFalse((self.output / "posts/first.html").exists())
        for name in ("index.html", "pages/renamed.html"):
            rendered = ThemePage(self.read(name))
            self.assertEqual(rendered.collections["site-posts"], [])
            self.assertEqual([item["data-title"] for item in rendered.collections["site-pages"]], ["After"])
            self.assert_links(rendered.collections["site-pages"], ["pages/renamed.html"],
                              "https://example.test/", name)

    def test_default_recent_posts_are_global_limited_escaped_and_optional(self):
        self.write_page("pages/deep/about.md", "About", "About body.")
        for index in range(6):
            self.write_page(f"posts/{index}.md", f"Older {index} <&>", "Body.", date="2025-01-01")
        self.config('paginate_by = 1\narchives = true\n[extra]\nrecent_posts_limit = 2\n')
        self.build()
        for name in ("index.html", "page/7/index.html", "archives/index.html",
                     "posts/first.html", "pages/deep/about.html"):
            rendered = ThemePage(self.read(name))
            self.assert_links(rendered.collections["recent"],
                              ["posts/first.html", "posts/0.html"], "https://example.test/blog/", name)
            self.assertIn("Older 0 &lt;&amp;&gt;", self.read(name))
        for options in ('show_recent_posts = false', 'recent_posts_limit = 0'):
            self.config('[extra]\n' + options + '\n')
            self.build()
            self.assertNotIn('class="recent-posts"', self.read("posts/first.html"))
            self.assertNotIn('class="with-sidebar"', self.read("posts/first.html"))
        self.write_page("posts/first.md", "Draft", "Draft.", draft=True)
        for path in self.content.glob("posts/[0-9].md"):
            path.unlink()
        self.config('')
        self.build()
        self.assertNotIn('class="recent-posts"', self.read("pages/deep/about.html"))

    def test_minimal_theme_builds_all_page_types_and_links_under_a_deployment_prefix(self):
        articles, pages = self.populate()
        # The example intentionally uses just the standard entry layouts.
        target = self.content / "pages/deep/关于 space.md"
        target.write_text(target.read_text(encoding="utf-8").replace('layout = "special.html"\n', ''),
                          encoding="utf-8")
        shutil.rmtree(self.theme)
        shutil.copytree(PROJECT_ROOT / "examples/minimal-theme", self.theme)
        base_url = "https://example.test/%E5%8D%9A%E5%AE%A2%20blog/"
        self.config(f'base_url = "{base_url}"\npaginate_by = 1\narchives = true\n')
        self.build()
        for path in self.output.rglob("*.html"):
            if path.name == "copied.html":
                continue
            name = path.relative_to(self.output).as_posix()
            source = path.read_text(encoding="utf-8")
            page = ThemePage(source)
            self.assert_links(page.collections["page-navigation"], pages, base_url, name)
            self.assert_links(page.collections["recent"], articles, base_url, name)
            for link in page.links:
                resolved = urljoin(base_url + name, link)
                self.assertTrue(resolved.startswith(base_url), (name, link))
                relative = resolved.removeprefix(base_url)
                if not relative or relative.endswith("/"):
                    relative += "index.html"
                self.assertTrue((self.output / unquote(relative)).is_file(), (name, link))
            self.assertNotIn("<script", source)
        self.assertIn("Body kept in the current page only.", self.read("posts/first.html"))
        self.assertIn("1 / 3", self.read())
        self.assertIn('rel="next"', self.read())
        self.assertIn('aria-current="page"', self.read("pages/deep/关于 space.html"))


if __name__ == "__main__":
    run_tests()
