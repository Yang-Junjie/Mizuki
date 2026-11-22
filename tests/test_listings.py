"""Paginated homepages, year archives and their shared publication rules."""

from html.parser import HTMLParser
import json
from urllib.parse import quote, unquote, urljoin, urlsplit
import xml.etree.ElementTree as ET

from support import BuilderFixture, run_tests


class Listing(HTMLParser):
    def __init__(self, source):
        super().__init__()
        self.post_links = []
        self.links = []
        self.years = []
        self.canonical = None
        self.in_post = False
        self.feed(source)

    def handle_starttag(self, tag, attrs):
        attrs = dict(attrs)
        if tag == "li" and attrs.get("class") == "post-item":
            self.in_post = True
        if tag == "h2" and attrs.get("id", "").startswith("year-"):
            self.years.append(attrs["id"][5:])
        if tag == "a":
            self.links.append(attrs)
            if self.in_post:
                self.post_links.append(attrs["href"])
        if tag == "link" and attrs.get("rel") == "canonical":
            self.canonical = attrs["href"]

    def handle_endtag(self, tag):
        if tag == "li":
            self.in_post = False

    def pagination(self):
        return {item["rel"]: item["href"] for item in self.links if item.get("rel") in ("prev", "next")}


class ListingTests(BuilderFixture):
    def configure(self, paginate_by=2, archives=True, base_url="https://example.com/blog/", **values):
        values = dict(paginate_by=paginate_by, archives=archives, base_url=base_url, **values)
        navigation = values.pop("navigation", [])
        text = "".join(f"{key} = {json.dumps(value, ensure_ascii=False)}\n" for key, value in values.items())
        for item in navigation:
            text += "\n[[navigation]]\n"
            text += "".join(f"{key} = {json.dumps(value, ensure_ascii=False)}\n" for key, value in item.items())
        (self.root / "mizuki.toml").write_text(text, encoding="utf-8")

    def populate(self):
        self.write_page("posts/zeta.md", "Latest <&", "Latest body.", date="2026-12-31")
        self.write_page("posts/alpha.md", "Alpha", "Alpha body.", date="2026-11-16")
        self.write_page("notes/中文 space.md", "Older", "Older body.", date="2025-01-02")
        self.write_page("posts/ancient.md", "Ancient", "Ancient body.", date="2024-02-29")
        self.write_page("posts/hidden.md", "Hidden", "Draft.", draft=True, date="2027-01-01")
        self.write_page("pages/about.md", "About", "Independent.")
        (self.content / "copied.html").write_text("Copied HTML.", encoding="utf-8")
        return ["posts/zeta.html", "posts/alpha.html", "posts/first.html",
                "notes/中文 space.html", "posts/ancient.html"]

    def read(self, path="index.html"):
        return (self.output / path).read_text(encoding="utf-8")

    def listing(self, path="index.html"):
        return Listing(self.read(path))

    def resolved_posts(self, path="index.html", prefix="/blog/"):
        return [unquote(urlsplit(urljoin(prefix + path, href)).path).removeprefix(prefix)
                for href in self.listing(path).post_links]

    def sitemap(self):
        return [node.text for node in ET.parse(self.output / "sitemap.xml").getroot().iter(
            "{http://www.sitemaps.org/schemas/sitemap/0.9}loc")]

    def assert_failure_preserves_output(self, *messages):
        before = self.snapshot()
        output = self.build(success=False)
        for message in messages:
            self.assertIn(message, output)
        self.assertEqual(self.snapshot(), before)
        self.assert_no_temporary_directories()

    def test_disabled_features_do_not_require_archive_layout(self):
        expected = self.populate()
        home = self.theme / "layouts/home.html"
        home.write_text('<title>Custom theme</title><ul>{% include "partials/posts.html" %}</ul>', encoding="utf-8")
        (self.theme / "layouts/archives.html").unlink()
        self.build()
        self.assertEqual(self.resolved_posts(prefix="/"), expected)
        self.assertFalse((self.output / "page").exists())
        self.assertFalse((self.output / "archives").exists())

    def test_pagination_order_boundaries_and_accessible_navigation(self):
        expected = self.populate()
        self.configure()
        self.build()
        paths = ["index.html", "page/2/index.html", "page/3/index.html"]
        self.assertEqual([post for path in paths for post in self.resolved_posts(path)], expected)
        for index, path in enumerate(paths):
            self.assertEqual(len(self.listing(path).post_links), (2, 2, 1)[index])
            self.assertIn(f'aria-current="page">第 {index + 1} / 3 页', self.read(path))
            self.assertIn('aria-label="文章分页"', self.read(path))
        self.assertEqual(self.listing().pagination(), {"next": "page/2/"})
        self.assertEqual(self.listing(paths[1]).pagination(), {"prev": "../../", "next": "../3/"})
        self.assertEqual(self.listing(paths[2]).pagination(), {"prev": "../2/"})
        self.assertIn("Latest &lt;&amp;", self.read())
        self.assertIn("%E4%B8%AD%E6%96%87%20space.html", self.read(paths[1]))
        self.assertIn("<title>Mizuki · 第 2 页</title>", self.read(paths[1]))
        self.assertFalse((self.output / "page/1").exists())

    def test_archive_groups_all_articles_and_excludes_drafts_pages_and_assets(self):
        expected = self.populate()
        self.configure()
        self.build()
        archive = self.listing("archives/index.html")
        self.assertEqual(archive.years, ["2026", "2025", "2024"])
        self.assertEqual(self.resolved_posts("archives/index.html"), expected)
        self.assertEqual(self.read("archives/index.html").count('id="year-2026"'), 1)
        self.assertIn('href="../assets/css/style.css"', self.read("archives/index.html"))
        self.assertIn('href="../../assets/css/style.css"', self.read("page/2/index.html"))

    def test_canonical_sitemap_and_complete_rss_share_one_publication_order(self):
        expected = self.populate()
        self.configure()
        self.build()
        root = "https://example.com/blog/"
        for path, url in (("index.html", ""), ("page/2/index.html", "page/2/"),
                          ("page/3/index.html", "page/3/"), ("archives/index.html", "archives/")):
            self.assertEqual(self.listing(path).canonical, root + url)
        urls = self.sitemap()
        self.assertEqual(len(urls), len(set(urls)))
        self.assertEqual(urls[:4], [root, root + "page/2/", root + "page/3/", root + "archives/"])
        self.assertNotIn(root + "page/1/", urls)
        self.assertFalse(any(url.endswith("/index.html") for url in urls))
        self.assertIn(root + "pages/about.html", urls)
        feed = ET.parse(self.output / "rss.xml").getroot().findall("channel/item/link")
        self.assertEqual([unquote(item.text).removeprefix(root) for item in feed], expected)
        before = self.snapshot()
        self.build()
        self.assertEqual(self.snapshot(), before)

    def test_generated_links_work_at_root_and_encoded_subpaths(self):
        expected = self.populate()
        for base in ("/", "/blog/", "https://example.com/中文%20space/"):
            with self.subTest(base=base):
                self.configure(base_url=base, navigation=[{"title": "首页", "url": "/"},
                                                       {"title": "归档", "url": "/archives/"}])
                self.build()
                prefix = quote(unquote(urlsplit(base).path), safe="/")
                for path in ("index.html", "page/2/index.html", "page/3/index.html", "archives/index.html"):
                    listing = self.listing(path)
                    for anchor in listing.links:
                        if anchor.get("class") == "feed-link":
                            self.assertEqual(anchor["href"], "https://example.com" + prefix + "rss.xml")
                            continue
                        url = urlsplit(urljoin("https://preview.test" + prefix + path, anchor["href"]))
                        self.assertEqual(url.netloc, "preview.test")
                        self.assertTrue(url.path.startswith(prefix), url.path)
                        relative = unquote(url.path.removeprefix(prefix))
                        target = self.output / relative
                        if url.path.endswith("/"):
                            target /= "index.html"
                        self.assertTrue(target.is_file(), (path, anchor))
                # The archive always preserves the complete publication order.
                self.assertEqual(self.resolved_posts("archives/index.html"), expected)

    def test_empty_site_has_one_homepage_and_an_empty_archive(self):
        (self.content / "posts/first.md").unlink()
        self.configure(paginate_by=1)
        self.build()
        self.assertEqual(self.listing().post_links, [])
        self.assertEqual(self.listing().pagination(), {})
        self.assertNotIn('class="pagination"', self.read())
        self.assertIn("暂无文章", self.read("archives/index.html"))
        self.assertFalse((self.output / "page").exists())
        self.assertEqual(self.sitemap(), ["https://example.com/blog/", "https://example.com/blog/archives/"])

    def test_single_and_exact_multiple_page_counts(self):
        self.write_page("posts/second.md", "Second", "Body.")
        for size, expected in ((0, 1), (1, 2), (2, 1), (3, 1), (9223372036854775807, 1)):
            with self.subTest(size=size):
                self.configure(paginate_by=size, archives=False)
                self.build()
                pages = list((self.output / "page").glob("*/index.html"))
                self.assertEqual(len(pages) + 1, expected)
                self.assertEqual(bool(self.listing().pagination()), expected > 1)
                self.assertFalse((self.output / "archives").exists())

    def test_theme_can_choose_its_own_pagination_markup(self):
        home = self.theme / "layouts/home.html"
        home.write_text(home.read_text(encoding="utf-8").replace(
            '{% include "partials/pagination.html" %}', ""), encoding="utf-8")
        self.configure(paginate_by=1)
        self.write_page("posts/second.md", "Second", "Body.")
        self.build()
        self.assertTrue((self.output / "page/2/index.html").is_file())
        self.assertFalse(self.listing().pagination())

    def test_archive_template_is_required_only_when_enabled(self):
        template = self.theme / "layouts/archives.html"
        source = template.read_text(encoding="utf-8")
        template.unlink()
        self.configure(archives=False)
        self.build()
        self.configure()
        self.assert_failure_preserves_output("Failed to load template", "archives.html")
        template.write_text('{{ missing_archive }}', encoding="utf-8")
        self.assert_failure_preserves_output("missing_archive", "archives.html")
        template.write_text(source, encoding="utf-8")
        self.build()
        self.assertTrue((self.output / "archives/index.html").is_file())

    def test_generated_outputs_conflict_with_content_and_case_aliases(self):
        self.populate()
        self.configure()
        self.build()
        for name in ("page/2/index.html", "page/2/index.md", "PAGE/2/INDEX.HTML", "page/2/index.html/child.bin",
                     "archives/index.md", "ARCHIVES/INDEX.HTML", "archives/index.html/child.bin"):
            with self.subTest(name=name):
                target = self.content / name
                target.parent.mkdir(parents=True, exist_ok=True)
                if name.endswith(".md"):
                    self.write_page(name, "Conflict", "Body.")
                else:
                    target.write_text("Conflict.", encoding="utf-8")
                self.assert_failure_preserves_output("Output path conflict", "generated", target.as_posix())
                target.unlink()
                for parent in list(target.parents):
                    if parent == self.content:
                        break
                    if not any(parent.iterdir()):
                        parent.rmdir()

    def test_generated_directories_conflict_with_static_files(self):
        self.populate()
        self.configure()
        self.build()
        for name in ("page", "page/2", "archives"):
            with self.subTest(name=name):
                target = self.content / name
                target.parent.mkdir(parents=True, exist_ok=True)
                target.write_text("Conflict.", encoding="utf-8")
                self.assert_failure_preserves_output("Output path conflict", "generated", target.as_posix())
                target.unlink()
                if target.parent != self.content:
                    target.parent.rmdir()

    def test_removed_articles_and_disabled_features_remove_stale_outputs(self):
        self.populate()
        self.configure()
        self.build()
        (self.content / "posts/ancient.md").unlink()
        self.build()
        self.assertFalse((self.output / "page/3").exists())
        self.assertEqual(len(self.listing("page/2/index.html").post_links), 2)
        self.write_page("posts/zeta.md", "Latest", "Draft.", draft=True)
        self.write_page("posts/alpha.md", "Alpha", "Draft.", draft=True)
        self.build()
        self.assertFalse((self.output / "page").exists())
        self.assertEqual(self.listing("archives/index.html").years, ["2026", "2025"])
        self.configure(paginate_by=0, archives=False)
        self.build()
        self.assertFalse((self.output / "archives").exists())
        self.assertNotIn("https://example.com/blog/archives/", self.sitemap())

    def test_invalid_configurations_preserve_previous_output(self):
        self.configure()
        self.build()
        for key, invalid in (("paginate_by", "-1"), ("paginate_by", "1.0"), ("paginate_by", '"2"'),
                             ("paginate_by", "true"), ("paginate_by", "[]"),
                             ("archives", '"true"'), ("archives", "1"), ("archives", "[]")):
            with self.subTest(key=key, invalid=invalid):
                (self.root / "mizuki.toml").write_text(f"{key} = {invalid}\n", encoding="utf-8")
                self.assert_failure_preserves_output(f"Configuration '{key}' must be")

    def test_listing_placeholders_are_replaced_once_and_escaped(self):
        self.populate()
        self.configure(title="Site {page_number} <&")
        home = self.theme / "layouts/home.html"
        home.write_text(home.read_text(encoding="utf-8").replace("</main>",
                        "<p>{{ pagination.current_page }}/{{ pagination.total_pages }}</p></main>"), encoding="utf-8")
        self.build()
        self.assertIn("Site {page_number} &lt;&amp;", self.read())
        self.assertIn("<p>2/3</p>", self.read("page/2/index.html"))

    def test_body_links_to_generated_directories_respect_deployment_prefix(self):
        self.populate()
        self.configure()
        self.write_page("pages/links.md", "Links", "[Archive](/archives/)\n\n[Page two](/page/2/)")
        self.build()
        links = Listing(self.read("pages/links.html")).links
        self.assertIn({"href": "/blog/archives/"}, links)
        self.assertIn({"href": "/blog/page/2/"}, links)


if __name__ == "__main__":
    run_tests()
