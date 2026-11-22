"""Theme authoring through data, configuration, inheritance and selected layouts."""


from support import BuilderFixture, run_tests


class ThemeTests(BuilderFixture):
    def read(self, path="index.html"):
        return (self.output / path).read_text(encoding="utf-8")

    def layout(self, name, text):
        path = self.theme / "layouts" / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text, encoding="utf-8")

    def config(self, text):
        (self.root / "mizuki.toml").write_text(text, encoding="utf-8")

    def selected_layout(self, path, layout):
        target = self.content / path
        target.write_text(target.read_text(encoding="utf-8").replace(
            "draft: false", "draft: false\nlayout: " + layout), encoding="utf-8")

    def fails_preserving(self, *messages):
        before = self.snapshot()
        error = self.build(success=False)
        for message in messages:
            self.assertIn(message, error)
        self.assertEqual(self.snapshot(), before)
        self.assert_no_temporary_directories()

    def test_same_data_drives_list_or_cards_without_changing_core(self):
        self.write_page("posts/first.md", '<First & "post">', "Body.", description="Summary <&>")
        self.write_page("posts/older.md", "Older", "Body.", date="2024-01-01")
        self.write_page("posts/hidden.md", "Draft", "Body.", draft=True)
        self.write_page("pages/about.md", "About", "Body.")
        self.config('base_url = "https://example.com/blog/"\npaginate_by = 1\n')
        self.layout("home.html", '<ul>{% for post in posts %}<li><a href="{{ post.url }}">'
                    '{{ post.title }}</a></li>{% endfor %}</ul>')
        self.build()
        self.assertIn('<a href="posts/first.html">&lt;First &amp; &quot;post&quot;&gt;</a>', self.read())
        self.assertIn('href="../../posts/older.html"', self.read("page/2/index.html"))
        self.assertNotIn("Draft", self.read())
        self.assertNotIn("About", self.read())
        self.layout("home.html", '{% for post in posts %}<article><h2>{{ post.title }}</h2>'
                    '{% if post.description != "" %}<p>{{ post.description }}</p>{% endif %}'
                    '<time>{{ post.date }}</time></article>{% endfor %}')
        self.build()
        self.assertIn('<p>Summary &lt;&amp;&gt;</p><time>2026-11-16</time>', self.read())
        self.assertNotIn('<p>', self.read("page/2/index.html"))

    def test_old_placeholders_are_literal_text(self):
        self.layout("home.html", '{title}|{posts}|{asset_base}|{{ site.title }}')
        self.build()
        self.assertEqual(self.read(), '{title}|{posts}|{asset_base}|Mizuki')

    def test_inheritance_includes_and_super_share_context(self):
        self.layout("base.html", '<html><title>{% block title %}Base{% endblock %}</title>'
                    '{% block content %}Default{% endblock %}</html>')
        self.layout("partials/card.html", '<article>{{ post.title }}</article>')
        self.layout("home.html", '{% extends "base.html" %}{% block title %}'
                    '{{ super() }} / {{ site.title }}{% endblock %}{% block content %}'
                    '{% for post in posts %}{% include "partials/card.html" %}{% endfor %}'
                    '{% endblock %}')
        self.build()
        self.assertIn('<title>Base / Mizuki</title><article>First post</article>', self.read())

    def test_extra_defaults_merge_recursively_and_preserve_types(self):
        self.layout("post.html", "{{ page.title }}")
        (self.theme / "theme.toml").write_text('''name = "Test"
engine = "inja"
[extra]
show = true
number = 2
ratio = 1.5
label = "Default"
links = ["default"]
[extra.colors]
accent = "blue"
background = "white"
''', encoding="utf-8")
        self.config('''[extra]
show = false
number = 3
label = "<User & value>"
links = ["one", "two"]
[extra.colors]
accent = "red"
''')
        self.layout("home.html", '{{ site.extra.number + 1 }} / {{ site.extra.ratio }} / '
                    '{{ site.extra.label }} / {{ site.extra.colors.accent }} / '
                    '{{ site.extra.colors.background }} / '
                    '{% if site.extra.show %}VISIBLE{% endif %}'
                    '{% for link in site.extra.links %}<i>{{ link }}</i>{% endfor %}')
        self.build()
        self.assertEqual(self.read(), '4 / 1.5 / &lt;User &amp; value&gt; / red / white / '
                         '<i>one</i><i>two</i>')
        (self.root / "mizuki.toml").unlink()
        self.build()
        self.assertIn('3 / 1.5 / Default / blue / white / VISIBLE<i>default</i>', self.read())

    def test_default_theme_options_control_toc_and_escaped_footer(self):
        self.write_page("posts/first.md", "Post", "## Section")
        self.config('[extra]\nshow_toc = false\nfooter_text = "<Footer>"\n')
        self.build()
        self.assertNotIn('class="toc"', self.read("posts/first.html"))
        self.assertIn('id="section"', self.read("posts/first.html"))
        self.assertIn('<footer><p>&lt;Footer&gt;</p>', self.read("posts/first.html"))

    def test_content_is_explicit_html_and_strings_are_never_reparsed(self):
        self.write_page("posts/first.md", '{{ site.title }} <Title>', '**Bold** and {{ site.title }}')
        self.layout("post.html", '<h1>{{ page.title }}</h1><raw>{{ html(page.content) }}</raw>'
                    '<escaped>{{ page.content }}</escaped>')
        self.build()
        text = self.read("posts/first.html")
        self.assertIn('<h1>{{ site.title }} &lt;Title&gt;</h1>', text)
        self.assertIn('<raw><p><strong>Bold</strong> and {{ site.title }}</p>', text)
        self.assertIn('<escaped>&lt;p&gt;&lt;strong&gt;Bold&lt;/strong&gt;', text)

    def test_custom_layout_for_articles_and_pages_does_not_require_defaults(self):
        self.write_page("pages/about.md", "About", "About body.")
        self.layout("special/project.html", '<h1>{{ page.kind }}: {{ page.title }}</h1>'
                    '{{ html(page.content) }}')
        for source in ("posts/first.md", "pages/about.md"):
            self.selected_layout(source, "special/project.html")
        (self.theme / "layouts/post.html").unlink()
        (self.theme / "layouts/page.html").unlink()
        self.build()
        self.assertIn('<h1>post: First post</h1>', self.read("posts/first.html"))
        self.assertIn('<h1>page: About</h1>', self.read("pages/about.html"))
        self.selected_layout("posts/first.md", "unused.html")  # duplicate key must fail
        self.fails_preserving("Duplicate front matter key: layout", "posts/first.md")

    def test_missing_selected_layout_and_unsafe_paths_fail_with_source(self):
        self.build()
        original = (self.content / "posts/first.md").read_text(encoding="utf-8")
        for name in ('missing.html', '../secret.html', '/tmp/secret.html', 'C:\\secret.html', ''):
            with self.subTest(layout=name):
                (self.content / "posts/first.md").write_text(original, encoding="utf-8")
                self.selected_layout("posts/first.md", name)
                self.fails_preserving("posts/first.md", "layout" if name == '' else name)

    def test_all_heading_data_and_nested_toc_are_available(self):
        self.write_page("posts/first.md", "Headings", '### Orphan\n\n## 安装\n\n### Step\n\n'
                        '# Reset\n\n### Alone\n\n#### Detail')
        self.layout("post.html", '{% for h in page.headings %}[{{ h.level }}:{{ h.id }}]'
                    '{% endfor %}{% for h in toc %}<a href="{{ h.url }}">{{ h.text }}</a>'
                    '{% for child in h.children %}<b>{{ child.text }}</b>{% endfor %}{% endfor %}')
        self.build()
        text = self.read("posts/first.html")
        self.assertIn('[4:detail]', text)
        self.assertIn('<a href="#%E5%AE%89%E8%A3%85">安装</a><b>Step</b>', text)
        self.assertIn('<a href="#orphan">Orphan</a>', text)
        self.assertIn('<a href="#alone">Alone</a>', text)

    def test_template_errors_and_cycles_preserve_output_and_recover(self):
        self.build()
        original = (self.theme / "layouts/home.html").read_text(encoding="utf-8")
        for source, message in (
            ('{{ missing.value }}', 'missing.value'),
            ('{% if %}', 'parse'),
            ('{% include "missing.html" %}', 'missing.html'),
            ('{% include "../outside.html" %}', 'inside theme layouts'),
            ('{% extends "loop.html" %}', 'cycle'),
            ('{{ html(posts) }}', 'html() requires a string'),
        ):
            with self.subTest(source=source):
                self.layout("loop.html", '{% include "home.html" %}')
                self.layout("home.html", source)
                self.fails_preserving(message, "home.html")
        self.layout("home.html", original)
        self.build()

    def test_manifest_and_extra_validation(self):
        self.build()
        for text in ('engine = "legacy"', 'extra = 1', 'engine = false', 'typo = true',
                     '[extra]\nvalue = nan', '[extra]\nvalue = 2026-11-22'):
            with self.subTest(manifest=text):
                (self.theme / "theme.toml").write_text(text, encoding="utf-8")
                self.fails_preserving("theme.toml")
        (self.theme / "theme.toml").unlink()
        for text in ('extra = 1', '[extra]\nvalue = inf', '[extra]\nvalue = 2026-11-22'):
            with self.subTest(config=text):
                self.config(text)
                self.fails_preserving("mizuki.toml")

    def test_symlink_template_cannot_escape_layouts(self):
        self.build()
        outside = self.root / "outside"
        outside.mkdir()
        (outside / "secret.html").write_text("SECRET", encoding="utf-8")
        self.make_symlink(self.theme / "layouts/outside", outside)
        self.layout("home.html", '{% include "outside/secret.html" %}')
        self.fails_preserving("escapes theme layouts")


if __name__ == "__main__":
    run_tests()
