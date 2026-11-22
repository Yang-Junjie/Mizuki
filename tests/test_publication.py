"""Publication metadata, feed, sitemap and canonical URL regressions."""

from email.utils import parsedate_to_datetime
from html import unescape
from html.parser import HTMLParser
import json
from urllib.parse import quote
import xml.etree.ElementTree as ET

from support import BuilderFixture, run_tests


class Head(HTMLParser):
    def __init__(self, source):
        super().__init__()
        self.links = []
        self.anchors = []
        self.meta = {}
        self.feed(source)

    def handle_starttag(self, tag, attrs):
        attrs = dict(attrs)
        if tag == "link":
            self.links.append(attrs)
        elif tag == "meta" and "name" in attrs:
            self.meta[attrs["name"]] = attrs.get("content")
        elif tag == "a":
            self.anchors.append(attrs)


class PublicationTests(BuilderFixture):
    def setUp(self):
        super().setUp()
        self.configure()

    def configure(self, base_url="https://example.com/blog/", **metadata):
        values = {"base_url": base_url, **metadata}
        (self.root / "mizuki.toml").write_text(
            "".join(f"{key} = {json.dumps(value, ensure_ascii=False)}\n" for key, value in values.items()),
            encoding="utf-8",
        )

    def head(self, name):
        return Head((self.output / name).read_text(encoding="utf-8"))

    def feed(self):
        return ET.parse(self.output / "rss.xml").getroot().find("channel")

    def sitemap(self):
        return [node.text for node in ET.parse(self.output / "sitemap.xml").getroot().findall(
            "{http://www.sitemaps.org/schemas/sitemap/0.9}url/"
            "{http://www.sitemaps.org/schemas/sitemap/0.9}loc"
        )]

    def independent(self, date=None, description=None):
        path = self.content / "pages/about.md"
        path.parent.mkdir(exist_ok=True)
        fields = "title: About\n"
        if date is not None:
            fields += f"date: {date}\n"
        if description is not None:
            fields += f"description: {description}\n"
        path.write_text(f"---\n{fields}---\nAbout body.\n", encoding="utf-8")

    def assert_failure_preserves_output(self, *messages):
        before = self.snapshot()
        output = self.build(success=False)
        for message in messages:
            self.assertIn(message, output)
        self.assertEqual(self.snapshot(), before)
        self.assert_no_temporary_directories()

    def test_dates_validate_calendar_and_rss_weekday_without_locale_or_clock(self):
        for date, expected in (
            ("2024-02-29", "Thu, 29 Feb 2024 00:00:00 GMT"),
            ("2000-02-29", "Tue, 29 Feb 2000 00:00:00 GMT"),
            ("1900-03-01", "Thu, 01 Mar 1900 00:00:00 GMT"),
            ("0001-01-01", "Mon, 01 Jan 0001 00:00:00 GMT"),
            ("9999-12-31", "Fri, 31 Dec 9999 00:00:00 GMT"),
        ):
            with self.subTest(date=date):
                self.write_page("posts/first.md", "Date", "Body.", date=date)
                self.build()
                actual = self.feed().findtext("item/pubDate")
                self.assertEqual(actual, expected)
                # Python's email parser interprets even four-digit years below 100 as 19xx/20xx.
                # The exact expected string above covers those calendar boundary cases.
                if int(date[:4]) >= 100:
                    self.assertEqual(parsedate_to_datetime(actual).isoformat(), f"{date}T00:00:00+00:00")

    def test_invalid_dates_preserve_output_and_report_source(self):
        self.build()
        for date in (
            "2026-02-29", "1900-02-29", "2026-04-31", "2026-00-01", "2026-13-01",
            "2026-01-00", "2026-01-32", "0000-01-01", "2026-1-01", "2026-01-1",
            "26-01-01", "2026/01/01", "2026-01-01T00:00:00Z", "not-a-date", "202x-11-22",
        ):
            with self.subTest(date=date):
                self.write_page("posts/first.md", "Invalid", "Body.", date=date)
                self.assert_failure_preserves_output("posts", "first.md", "Invalid date", "YYYY-MM-DD")

    def test_independent_page_date_is_optional_but_validated_when_present(self):
        for date in (None, "2024-02-29"):
            self.independent(date=date)
            self.build()
            self.assertEqual(len(self.feed().findall("item")), 1)
            self.assertIn("https://example.com/blog/pages/about.html", self.sitemap())
        self.independent(date="2026-02-30")
        self.assert_failure_preserves_output("about.md", "Invalid date")

    def test_description_is_optional_escaped_and_falls_back_to_site(self):
        fallback = '站点 & "description" <safe>'
        summary = '文章 & "summary" <safe> {site_title} ]]> 🌿'
        self.configure(description=fallback)
        self.write_page("posts/first.md", "First", "Body.", description=summary)
        self.independent(description="")
        self.build()
        self.assertEqual(self.head("index.html").meta["description"], fallback)
        self.assertEqual(self.head("posts/first.html").meta["description"], summary)
        self.assertEqual(self.head("pages/about.html").meta["description"], fallback)
        rss_summary = self.feed().findtext("item/description")
        self.assertEqual(unescape(rss_summary), summary)
        self.assertNotIn("<safe>", rss_summary)
        self.write_page("posts/first.md", "First", "Body.")
        self.build()
        self.assertEqual(self.head("posts/first.html").meta["description"], fallback)
        self.assertIsNone(self.feed().find("item/description"))

    def test_duplicate_description_is_rejected_with_source_context(self):
        self.build()
        path = self.content / "posts/first.md"
        path.write_text("---\ntitle: Duplicate\ndate: 2026-11-22\ndescription: One\n"
                        "description: Two\n---\nBody.\n", encoding="utf-8")
        self.assert_failure_preserves_output("first.md", "Duplicate front matter key: description")

    def test_rss_has_required_channel_data_stable_order_and_only_published_articles(self):
        self.configure(title='Mizuki & "朋友" <blog>', description="Read & write", language="zh-CN")
        self.write_page("posts/z.md", "Z", "Body.", date="2026-11-17")
        self.write_page("posts/a.md", "A", "Body.", date="2026-11-17", description="A summary")
        self.write_page("posts/hidden.md", "Hidden", "Draft.", draft=True, date="2030-01-01")
        self.independent(date="2030-01-01")
        (self.content / "raw.html").write_text("Static HTML.", encoding="utf-8")
        self.build()
        channel = self.feed()
        self.assertEqual(channel.findtext("title"), 'Mizuki & "朋友" <blog>')
        self.assertEqual(channel.findtext("description"), "Read & write")
        self.assertEqual(channel.findtext("language"), "zh-CN")
        self.assertEqual(channel.findtext("link"), "https://example.com/blog/")
        self.assertEqual(channel.find("{http://www.w3.org/2005/Atom}link").attrib,
                         {"href": "https://example.com/blog/rss.xml", "rel": "self",
                          "type": "application/rss+xml"})
        items = channel.findall("item")
        self.assertEqual([item.findtext("title") for item in items], ["A", "Z", "First post"])
        for item in items:
            self.assertEqual(item.find("guid").attrib, {"isPermaLink": "true"})
            self.assertEqual(item.findtext("guid"), item.findtext("link"))
        homepage = (self.output / "index.html").read_text(encoding="utf-8")
        self.assertLess(homepage.index('href="posts/a.html"'), homepage.index('href="posts/z.html"'))
        self.assertLess(homepage.index('href="posts/z.html"'), homepage.index('href="posts/first.html"'))
        self.assertEqual(set(self.sitemap()), {
            "https://example.com/blog/", "https://example.com/blog/posts/a.html",
            "https://example.com/blog/posts/z.html", "https://example.com/blog/posts/first.html",
            "https://example.com/blog/pages/about.html",
        })

    def test_publication_urls_agree_across_root_subpath_unicode_and_encoded_paths(self):
        special = "posts/中文 space#&%.md"
        self.write_page(special, "中文 & 🌿", "Body.")
        encoded = quote(special.removesuffix(".md") + ".html", safe="/")
        self.independent()
        for base, root in (
            ("https://example.com", "https://example.com/"),
            ("https://example.com/blog", "https://example.com/blog/"),
            ("https://example.com/中文 space/", "https://example.com/" + quote("中文 space/", safe="/")),
            ("https://example.com/%E4%B8%AD%E6%96%87%20space/", "https://example.com/" + quote("中文 space/", safe="/")),
        ):
            with self.subTest(base=base):
                self.configure(base_url=base)
                self.build()
                self.assertIn(root + encoded, self.sitemap())
                self.assertIn(root + encoded, [item.findtext("link") for item in self.feed().findall("item")])
                for page, canonical in (("index.html", root), (special.removesuffix(".md") + ".html", root + encoded),
                                        ("pages/about.html", root + "pages/about.html")):
                    head = self.head(page)
                    self.assertIn({"rel": "canonical", "href": canonical}, head.links)
                    alternate = [link for link in head.links if link.get("rel") == "alternate"]
                    self.assertEqual(len(alternate), 1)
                    self.assertEqual(alternate[0]["href"], root + "rss.xml")
                    self.assertEqual(alternate[0]["type"], "application/rss+xml")
                    self.assertIn({"class": "feed-link", "href": root + "rss.xml"}, head.anchors)

    def test_path_only_configuration_skips_publication_and_removes_previous_artifacts(self):
        self.build()
        for base in ("/", "/blog/", "/中文 space/"):
            with self.subTest(base=base):
                self.configure(base_url=base)
                message = self.build()
                self.assertIn("Skipping RSS, sitemap and canonical URLs", message)
                self.assertFalse((self.output / "rss.xml").exists())
                self.assertFalse((self.output / "sitemap.xml").exists())
                for page in ("index.html", "posts/first.html"):
                    head = self.head(page)
                    self.assertFalse(any(link.get("rel") in ("canonical", "alternate") for link in head.links))
                    self.assertFalse(any(anchor.get("class") == "feed-link" for anchor in head.anchors))
        self.configure()
        self.build()
        self.assertTrue((self.output / "rss.xml").is_file())
        self.assertTrue((self.output / "sitemap.xml").is_file())

    def test_generated_xml_conflicts_are_detected_before_publication(self):
        self.build()
        for name in ("rss.xml", "sitemap.xml", "RSS.XML", "SITEMAP.XML", "rss.xml/nested.bin", "sitemap.xml/nested.bin"):
            with self.subTest(name=name):
                source = self.content / name
                source.parent.mkdir(parents=True, exist_ok=True)
                source.write_text("Conflicting content.", encoding="utf-8")
                self.assert_failure_preserves_output("Output path conflict", "generated", name.split("/")[0])
                source.unlink()
                if source.parent != self.content:
                    source.parent.rmdir()

    def test_static_xml_is_allowed_without_origin_and_blocks_enabling_generation(self):
        self.configure(base_url="/blog/")
        (self.content / "rss.xml").write_text("Custom feed.", encoding="utf-8")
        (self.content / "sitemap.xml").write_text("Custom sitemap.", encoding="utf-8")
        self.build()
        self.assertEqual((self.output / "rss.xml").read_text(), "Custom feed.")
        self.configure()
        self.assert_failure_preserves_output("Output path conflict", "generated RSS feed", "rss.xml")

    def test_sitemap_url_length_limit_fails_without_replacing_output(self):
        self.build()
        self.configure(base_url="https://example.com/" + "prefix/" * 300)
        self.assert_failure_preserves_output("Sitemap URL must be shorter than 2048 characters")

    def test_empty_feed_and_sitemap_are_valid_and_builds_are_deterministic(self):
        (self.content / "posts/first.md").unlink()
        self.write_page("posts/hidden.md", "Hidden", "Draft.", draft=True)
        self.configure(description="")
        self.build()
        self.assertEqual(self.feed().findall("item"), [])
        self.assertEqual(self.feed().findtext("description"), "Mizuki")
        self.assertEqual(self.sitemap(), ["https://example.com/blog/"])
        before = self.snapshot()
        self.build()
        self.assertEqual(self.snapshot(), before)

    def test_invalid_xml_characters_or_utf8_fail_without_replacing_output(self):
        self.build()
        path = self.content / "posts/first.md"
        for invalid in (b"\x01", b"\x00", "\ufffe".encode(), b"\xc0\xaf", b"\xed\xa0\x80", b"\xf4\x90\x80\x80", b"\xe4\xb8"):
            with self.subTest(invalid=invalid):
                path.write_bytes(b"---\ntitle: First\ndate: 2026-11-22\ndescription: " + invalid + b"\n---\nBody.\n")
                self.assert_failure_preserves_output("Invalid UTF-8 or XML character")

    def test_optional_publication_placeholders_are_escaped_and_replaced_once(self):
        self.write_page("posts/first.md", "First", "Body.", description="{site_title} & <summary>")
        self.configure(title="Site")
        for name in ("home", "post"):
            path = self.theme / f"layouts/{name}.html"
            path.write_text(path.read_text(encoding="utf-8") +
                            '<p>{description}</p><p>{canonical_url}</p><p>{feed_url}</p>', encoding="utf-8")
        self.build()
        post = (self.output / "posts/first.html").read_text(encoding="utf-8")
        self.assertIn("<p>{site_title} &amp; &lt;summary&gt;</p>", post)
        self.assertIn("<p>https://example.com/blog/posts/first.html</p>", post)
        self.assertIn("<p>https://example.com/blog/rss.xml</p>", post)
        self.configure(base_url="/")
        self.build()
        self.assertIn("<p></p><p></p>", (self.output / "posts/first.html").read_text(encoding="utf-8"))


if __name__ == "__main__":
    run_tests()
