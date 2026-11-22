"""Typed document metadata, strict validation and default theme integration."""

from support import BuilderFixture, run_tests


class FrontMatterTests(BuilderFixture):
    def document(self, name, metadata, body="## Section\n\nBody."):
        path = self.content / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(f"+++\n{metadata}\n+++\n{body}", encoding="utf-8")

    def layout(self, name, text):
        (self.theme / "layouts" / name).write_text(text, encoding="utf-8")

    def read(self, name="index.html"):
        return (self.output / name).read_text(encoding="utf-8")

    def config(self, text):
        (self.root / "mizuki.toml").write_text(text, encoding="utf-8")

    def test_typed_extra_reaches_documents_pagination_and_archives(self):
        self.config('paginate_by = 1\narchives = true\n[extra]\nsite_only = "Site"\n')
        self.document("posts/first.md", '''title = "First"
date = "2026-11-22"
description = "Summary"
layout = "special.html"
[extra]
label = '<User & {{ site.title }}>'
show = false
count = 3
ratio = 1.25
links = ["one", "two"]
[extra.author]
name = "Artichoke"
[[extra.projects]]
name = "Mizuki"
''')
        self.document("posts/older.md", '''title = "Older"
date = 2025-01-01
[extra]
label = "Older metadata"
''')
        self.document("pages/about.md", '''title = "About"
[extra]
label = "Page metadata"
''')
        self.layout("special.html", '{{ page.extra.count + 1 }} / {{ page.extra.ratio }} / '
                    '{{ page.extra.label }} / {{ page.extra.author.name }} / '
                    '{% if page.extra.show %}VISIBLE{% endif %}'
                    '{% for link in default(page.extra.links, []) %}<i>{{ link }}</i>{% endfor %}'
                    '{% for project in page.extra.projects %}<b>{{ project.name }}</b>{% endfor %}'
                    '{{ default(page.extra.site_only, "Page only") }} / {{ site.extra.site_only }}')
        self.layout("page.html", '{{ page.extra.label }}|{{ page.date }}'
                    '{{ default(page.extra.author.name, "") }}'
                    '{% for link in default(page.extra.links, []) %}{{ link }}{% endfor %}')
        card = '<a href="{{ post.url }}">{{ post.extra.label }}</a>'
        self.layout("home.html", '{% for post in posts %}' + card + '{% endfor %}')
        self.layout("archives.html", '{% for group in archives %}{% for post in group.posts %}'
                    + card + '{% endfor %}{% endfor %}')
        self.build()
        escaped = '&lt;User &amp; {{ site.title }}&gt;'
        self.assertEqual(self.read("posts/first.html"), '4 / 1.25 / ' + escaped
                         + ' / Artichoke / <i>one</i><i>two</i><b>Mizuki</b>Page only / Site')
        self.assertEqual(self.read("pages/about.html"), 'Page metadata|')
        self.assertEqual(self.read(), '<a href="posts/first.html">' + escaped + '</a>')
        self.assertEqual(self.read("page/2/index.html"),
                         '<a href="../../posts/older.html">Older metadata</a>')
        self.assertIn('<a href="../posts/first.html">' + escaped + '</a>',
                      self.read("archives/index.html"))
        self.assertIn('<a href="../posts/older.html">Older metadata</a>',
                      self.read("archives/index.html"))

    def test_empty_extra_is_an_object_for_literal_toml_and_generated_pages(self):
        self.config('archives = true\n')
        self.document("pages/plain.md", 'title = "Plain"')
        self.document("posts/toml.md", 'title = "TOML"\ndate = "2026-11-22"')
        for name in ("home.html", "post.html", "page.html", "archives.html"):
            self.layout(name, '{{ isObject(page.extra) }}:{{ length(page.extra) }}'
                        '{% for post in posts %}|{{ isObject(post.extra) }}:'
                        '{{ length(post.extra) }}{% endfor %}')
        self.build()
        for name in ("posts/first.html", "posts/toml.html", "pages/plain.html",
                     "archives/index.html"):
            self.assertEqual(self.read(name), "true:0")
        self.assertEqual(self.read(), "true:0|true:0|true:0")

    def test_toml_drafts_are_validated_but_not_published(self):
        self.config('archives = true\nbase_url = "https://example.test/blog/"\n')
        self.document("posts/hidden.md", 'title = "Secret draft"\ndate = "2026-11-22"\ndraft = true')
        self.build()
        self.assertFalse((self.output / "posts/hidden.html").exists())
        for name in ("index.html", "archives/index.html", "rss.xml", "sitemap.xml"):
            self.assertNotIn("Secret draft", self.read(name))
            self.assertNotIn("hidden.html", self.read(name))
        self.document("posts/hidden.md", 'title = "Secret draft"\ndraft = true')
        before = self.snapshot()
        self.assertIn("date", self.build(success=False))
        self.assertEqual(self.snapshot(), before)

    def test_invalid_toml_metadata_preserves_output_and_reports_source(self):
        self.build()
        before = self.snapshot()
        invalid = (
            ('date = "2026-11-22"', "title"),
            ('title = "Post"', "date"),
            ('title = 1\ndate = "2026-11-22"', "title"),
            ('title = "  "\ndate = "2026-11-22"', "title"),
            ('title = "Post"\ndate = true', "date"),
            ('title = "Post"\ndate = ""', "date"),
            ('title = "Post"\ndate = "2026-02-29"', "date"),
            ('title = "Post"\ndate = "0000-01-01"', "date"),
            ('title = "Post"\ndate = 2026-11-22T12:00:00Z', "date"),
            ('title = "Post"\ndate = "2026-11-22"\ndescription = []', "description"),
            ('title = "Post"\ndate = "2026-11-22"\nlayout = false', "layout"),
            ('title = "Post"\ndate = "2026-11-22"\nlayout = "  "', "layout"),
            ('title = "Post"\ndate = "2026-11-22"\ndraft = "false"', "draft"),
            ('title = "Post"\ndate = "2026-11-22"\nextra = []', "extra"),
            ('title = "Post"\ndate = "2026-11-22"\ncover = "a.png"', "[extra]"),
            ('title = "Post"\ndate = "2026-11-22"\n[extra]\nbad = inf', "finite"),
            ('title = "Post"\ndate = "2026-11-22"\n[extra]\nbad = [1, nan]', "finite"),
            ('title = "Post"\ndate = "2026-11-22"\n[extra]\nbad = 2026-11-22', "quote"),
            ('title = "Post"\ndate = "2026-11-22"\n[extra.nested]\nbad = 12:00:00', "quote"),
            ('title = "Post"\ntitle = "Duplicate"\ndate = "2026-11-22"', "title"),
            ('title = "Post"\ndate = "2026-11-22"\n[extra]\nvalue = [', "line"),
        )
        for metadata, message in invalid:
            with self.subTest(metadata=metadata):
                self.document("posts/first.md", metadata)
                error = self.build(success=False)
                self.assertIn("posts/first.md", error)
                self.assertIn(message, error)
                self.assertEqual(self.snapshot(), before)
                self.assert_no_temporary_directories()
        self.document("posts/first.md", 'title = "Recovered"\ndate = "2026-11-22"')
        self.build()
        self.assertIn("Recovered", self.read())

    def test_optional_page_date_is_still_validated(self):
        self.build()
        for date in ('""', '"2026-02-29"', 'false'):
            with self.subTest(date=date):
                self.document("pages/about.md", 'title = "About"\ndate = ' + date)
                self.assertIn("date", self.build(success=False))

    def test_toml_delimiter_and_error_line_numbers(self):
        path = self.content / "posts/first.md"
        path.write_text('+++\ntitle = "Post"\ndate = "2026-11-22"\n---\nBody.', encoding="utf-8")
        self.assertIn("missing closing '+++'", self.build(success=False))
        self.document("posts/first.md", 'title = "Post"\ndate = "2026-11-22"\nbroken = @')
        self.assertIn("line 4", self.build(success=False))

    def test_page_toc_overrides_site_without_changing_heading_data(self):
        for site_show in (True, False):
            self.config('[extra]\nshow_toc = ' + str(site_show).lower())
            for page_show in (None, True, False):
                with self.subTest(site=site_show, page=page_show):
                    metadata = 'title = "Post"\ndate = "2026-11-22"'
                    if page_show is not None:
                        metadata += '\n[extra]\nshow_toc = ' + str(page_show).lower()
                    self.document("posts/first.md", metadata)
                    self.document("pages/about.md", metadata)
                    self.build()
                    for name in ("posts/first.html", "pages/about.html"):
                        page = self.read(name)
                        self.assertEqual('class="toc"' in page,
                                         site_show if page_show is None else page_show)
                        self.assertIn('id="section"', page)

    def test_covers_work_in_details_pagination_archives_and_deployment_prefixes(self):
        for prefix in ("/", "/Mizuki/", "/%E6%96%87%E6%A1%A3%20%E7%AB%99/"):
            with self.subTest(prefix=prefix):
                self.config(f'base_url = "https://example.test{prefix}"\n'
                            'paginate_by = 1\narchives = true\n')
                self.document("posts/deep/cover.md", '''title = "Covered"
date = "2026-11-01"
[extra]
cover = "images/封面 image.webp?size=2&mode=fit"
cover_alt = '<Cover & "alt">'
''')
                self.document("pages/project.md", '''title = "Project"
[extra]
cover = "https://images.example.test/cover.webp"
''')
                image = self.content / "images/封面 image.webp"
                image.parent.mkdir(exist_ok=True)
                image.write_bytes(b"cover")
                self.build()
                expected = prefix + 'images/%E5%B0%81%E9%9D%A2%20image.webp?size=2&amp;mode=fit'
                for name in ("posts/deep/cover.html", "page/2/index.html", "archives/index.html"):
                    self.assertIn(f'src="{expected}"', self.read(name))
                self.assertIn('alt="&lt;Cover &amp; &quot;alt&quot;&gt;"',
                              self.read("posts/deep/cover.html"))
                self.assertIn('src="https://images.example.test/cover.webp"',
                              self.read("pages/project.html"))
                self.assertNotIn('class="post-cover"', self.read())
                self.assertNotIn('class="page-cover"', self.read("posts/first.html"))
                self.assertEqual((self.output / image.relative_to(self.content)).read_bytes(), b"cover")

    def test_url_helper_encodes_site_paths_and_leaves_external_urls_and_suffixes(self):
        self.config('base_url = "https://example.test/blog/"')
        self.layout("home.html", '{{ url("/images/封面 image.webp?x=1&y=2#photo") }}|'
                    '{{ url("images/a%20b.png") }}|{{ url("/") }}|'
                    '{{ url("https://cdn.example.test/image.webp") }}|'
                    '{{ url("//cdn.example.test/image.webp") }}|'
                    '{{ url("#section") }}|{{ url("?page=2") }}')
        self.build()
        self.assertEqual(self.read(), '/blog/images/%E5%B0%81%E9%9D%A2%20image.webp?x=1&amp;y=2#photo|'
                         '/blog/images/a%20b.png|/blog/|https://cdn.example.test/image.webp|'
                         '//cdn.example.test/image.webp|#section|?page=2')
        before = self.snapshot()
        for expression in ('false', '"../outside.png"', '"%2e%2e/outside.png"',
                           '"/%2foutside.png"'):
            with self.subTest(expression=expression):
                self.layout("home.html", '{{ url(' + expression + ') }}')
                self.assertIn("url()", self.build(success=False))
                self.assertEqual(self.snapshot(), before)


if __name__ == "__main__":
    run_tests()
