"""Article taxonomies, generated routes, theme contexts and transactional rebuilds."""

from html.parser import HTMLParser
from pathlib import Path
from urllib.parse import quote, unquote, urljoin, urlsplit
import json
import shutil
import subprocess
import xml.etree.ElementTree as ET

from support import BuilderFixture, PROJECT_ROOT, run_tests


class Page(HTMLParser):
    def __init__(self, source):
        super().__init__(convert_charrefs=True)
        self.main = {}
        self.links = []
        self.canonicals = []
        self.feed(source)

    def handle_starttag(self, tag, attributes):
        attributes = dict(attributes)
        if tag == "main":
            self.main = attributes
        if tag == "link" and attributes.get("rel") == "canonical":
            self.canonicals.append(attributes["href"])
        if tag == "a":
            self.links.append(attributes)

    def scope(self, name):
        return [link for link in self.links if link.get("data-scope") == name]


class TaxonomyTests(BuilderFixture):
    def config(self, text='paginate_by = 2\narchives = true\nbase_url = "https://example.test/blog/"'):
        (self.root / "mizuki.toml").write_text(text, encoding="utf-8")

    def document(self, name, *, title="Article", date="2026-11-22", tags=(), categories=(),
                 draft=False, extra="", body="Article body."):
        metadata = [f"title = {json.dumps(title, ensure_ascii=False)}",
                    f"draft = {str(draft).lower()}"]
        if date is not None:
            metadata.append(f'date = "{date}"')
        metadata.extend([f"tags = {json.dumps(tags, ensure_ascii=False)}",
                         f"categories = {json.dumps(categories, ensure_ascii=False)}"])
        target = self.content / name
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_text("+++\n" + "\n".join(metadata) + "\n" + extra + "\n+++\n" + body,
                          encoding="utf-8")

    def read(self, path="index.html"):
        return (self.output / path).read_text(encoding="utf-8")

    def page(self, path="index.html"):
        return Page(self.read(path))

    def probe_theme(self):
        source = '''<main data-kind="{{ page.kind }}" data-taxonomy="{{ taxonomy.name }}"
 data-term="{{ term.name }}" data-count="{{ term.count }}" data-number="{{ pagination.current_page }}"
 data-total="{{ pagination.total_pages }}" data-global-posts="{{ length(site.posts) }}"
 data-extra="{{ join(default(page.extra.tags, []), "|") }}">
<link rel="canonical" href="{{ page.canonical_url }}">
<a data-scope="overview" href="{{ taxonomy.url }}">{{ taxonomy.title }}</a>
<a data-scope="term" href="{{ term.url }}">{{ term.name }}</a>
<a data-scope="previous" href="{{ pagination.previous_url }}">Previous</a>
<a data-scope="next" href="{{ pagination.next_url }}">Next</a>
{% for entry in site.tags %}<a data-scope="site-tags" data-name="{{ entry.name }}" data-count="{{ entry.count }}" href="{{ entry.url }}"></a>{% endfor %}
{% for entry in site.categories %}<a data-scope="site-categories" data-name="{{ entry.name }}" data-count="{{ entry.count }}" href="{{ entry.url }}"></a>{% endfor %}
{% for entry in page.tags %}<a data-scope="page-tags" data-name="{{ entry.name }}" href="{{ entry.url }}"></a>{% endfor %}
{% for entry in page.categories %}<a data-scope="page-categories" data-name="{{ entry.name }}" href="{{ entry.url }}"></a>{% endfor %}
{% for entry in terms %}<a data-scope="terms" data-name="{{ entry.name }}" data-count="{{ entry.count }}" href="{{ entry.url }}"></a>{% endfor %}
{% for entry in posts %}<a data-scope="posts" data-name="{{ entry.title }}" href="{{ entry.url }}"></a>{% endfor %}
{% for article in site.posts %}{% for entry in article.tags %}<a data-scope="summary-tags" data-owner="{{ article.title }}" data-name="{{ entry.name }}" href="{{ entry.url }}"></a>{% endfor %}{% endfor %}
{% for article in posts %}{% for entry in article.categories %}<a data-scope="slice-categories" data-owner="{{ article.title }}" data-name="{{ entry.name }}" href="{{ entry.url }}"></a>{% endfor %}{% endfor %}
{% for group in archives %}{% for article in group.posts %}{% for entry in article.tags %}<a data-scope="archive-tags" data-owner="{{ article.title }}" data-name="{{ entry.name }}" href="{{ entry.url }}"></a>{% endfor %}{% endfor %}{% endfor %}
</main>'''
        for name in ("home", "post", "page", "archives", "taxonomy", "term"):
            (self.theme / f"layouts/{name}.html").write_text(source, encoding="utf-8")

    def populate(self):
        self.document("posts/first.md", title="First", tags=["C++", "中文"], categories=["Tutorials"])
        self.document("posts/a.md", title="A", date="2025-01-01", tags=["C++"], categories=["Tutorials"])
        self.document("notes/z.md", title="Z", date="2025-01-01", tags=["C++", "Other"])
        self.document("posts/old.md", title="Old", date="2020-01-01", tags=["C++"], categories=["Notes"])
        self.document("posts/draft.md", title="Hidden", tags=["C++", "Draft only"],
                      categories=["Hidden category"], draft=True)
        self.document("pages/about.md", title="About", date=None)

    def assert_failure_preserves(self, *messages):
        before = self.snapshot()
        error = self.build(success=False)
        for message in messages:
            self.assertIn(message, error)
        self.assertEqual(self.snapshot(), before)
        self.assert_no_temporary_directories()

    def assert_links_exist(self, base="https://example.test/blog/"):
        for file in self.output.rglob("*.html"):
            current = urljoin(base, quote(file.relative_to(self.output).as_posix(), safe="/"))
            for link in Page(file.read_text(encoding="utf-8")).links:
                href = link.get("href", "")
                if not href:
                    continue
                resolved = urlsplit(urljoin(current, href))
                if resolved.netloc != urlsplit(base).netloc:
                    continue
                self.assertTrue(resolved.path.startswith(urlsplit(base).path), (file, href))
                target = unquote(resolved.path[len(urlsplit(base).path):])
                if not target or target.endswith("/"):
                    target += "index.html"
                self.assertTrue((self.output / target).is_file(), (file, href, target))

    def test_global_and_current_scopes_counts_sorting_and_pagination(self):
        self.config()
        self.populate()
        self.probe_theme()
        self.build()
        paths = ["index.html", "page/2/index.html", "posts/first.html", "pages/about.html",
                 "archives/index.html", "tags/index.html", "categories/index.html",
                 "tags/C++/index.html", "tags/C++/page/2/index.html", "categories/Tutorials/index.html"]
        expected = [("C++", "4"), ("Other", "1"), ("中文", "1")]
        for path in paths:
            with self.subTest(path=path):
                page = self.page(path)
                self.assertEqual([(e["data-name"], e["data-count"]) for e in page.scope("site-tags")], expected)
                self.assertEqual([(e["data-name"], e["data-count"]) for e in page.scope("site-categories")],
                                 [("Notes", "1"), ("Tutorials", "2")])
                self.assertEqual(page.main["data-global-posts"], "4")
                self.assertEqual(len(page.scope("summary-tags")), 6)
        self.assertEqual([e["data-name"] for e in self.page("tags/index.html").scope("terms")],
                         [e[0] for e in expected])
        self.assertEqual(self.page("tags/index.html").main["data-kind"], "taxonomy")
        self.assertEqual(self.page("categories/index.html").main["data-taxonomy"], "categories")
        first = self.page("tags/C++/index.html")
        second = self.page("tags/C++/page/2/index.html")
        # Same-date posts are ordered by their output path, just like the homepage.
        self.assertEqual([e["data-name"] for e in first.scope("posts")], ["First", "Z"])
        self.assertEqual([e["data-name"] for e in second.scope("posts")], ["A", "Old"])
        self.assertEqual(first.main["data-count"], "4")
        self.assertEqual(first.main["data-total"], "2")
        self.assertEqual(second.main["data-number"], "2")
        self.assertEqual(first.scope("next")[0]["href"], "page/2/")
        self.assertEqual(second.scope("previous")[0]["href"], "../../")
        self.assertEqual(second.scope("term")[0]["href"], "../../")
        self.assertEqual(second.scope("overview")[0]["href"], "../../../")
        self.assertEqual(len(self.page("archives/index.html").scope("archive-tags")), 6)
        self.assertFalse(self.page("pages/about.html").scope("page-tags"))
        self.assertFalse(first.scope("terms"))
        self.assertFalse(self.page("tags/index.html").scope("posts"))
        self.assert_links_exist()

    def test_pagination_boundaries_and_unpaginated_terms(self):
        self.populate()
        self.probe_theme()
        for size, counts in ((0, [4]), (4, [4]), (3, [3, 1]), (1, [1, 1, 1, 1])):
            with self.subTest(size=size):
                self.config(f"paginate_by = {size}")
                self.build()
                for index, count in enumerate(counts):
                    path = "tags/C++/" + (f"page/{index + 1}/" if index else "") + "index.html"
                    page = self.page(path)
                    self.assertEqual(len(page.scope("posts")), count)
                    self.assertEqual(page.main["data-total"], str(len(counts)))
                self.assertEqual((self.output / "tags/C++/page").exists(), len(counts) > 1)
                self.assertEqual((self.output / "categories/Tutorials/page/2/index.html").exists(),
                                 size == 1)
                self.assert_links_exist()
        self.config("paginate_by = 0")
        self.build()
        self.assertFalse((self.output / "tags/C++/page").exists())
        self.assertFalse((self.output / "categories/Tutorials/page").exists())

    def test_metadata_normalization_and_extra_stays_independent(self):
        self.probe_theme()
        self.document("posts/first.md", tags=[" B ", "A", "B", " A "],
                      categories=[" Notes ", "Notes"], extra='[extra]\ntags = ["theme-only"]')
        self.build()
        page = self.page("posts/first.html")
        self.assertEqual([e["data-name"] for e in page.scope("page-tags")], ["B", "A"])
        self.assertEqual([e["data-name"] for e in page.scope("page-categories")], ["Notes"])
        self.assertEqual(page.main["data-extra"], "theme-only")
        self.assertEqual([e["data-count"] for e in page.scope("site-tags")], ["1", "1"])
        self.assertFalse((self.output / "tags/theme-only").exists())

    def test_unicode_percent_spaces_deployment_prefix_and_sitemap(self):
        base = "https://example.test/%E5%8D%9A%E5%AE%A2%20blog/"
        self.config(f'base_url = "{base}"\npaginate_by = 1')
        tags = ["C++", "C#", "中文 space", "R&D 'notes'", "100%", "a%2Fb"]
        self.document("posts/first.md", tags=tags, categories=["分类"])
        self.document("deep/第二篇.md", title="Second", tags=["C++"], date="2020-01-01")
        self.build()
        self.assert_links_exist(base)
        post = self.read("posts/first.html")
        self.assertIn("R&amp;D", post)
        for name in tags:
            self.assertTrue((self.output / "tags" / name / "index.html").is_file())
            self.assertIn("../tags/" + quote(name, safe="") + "/", post)
        self.assertEqual(self.page("tags/C++/page/2/index.html").canonicals,
                         [base + "tags/C%2B%2B/page/2/"])
        urls = [node.text for node in ET.parse(self.output / "sitemap.xml").findall("{*}url/{*}loc")]
        expected = {base, base + "page/2/", base + "posts/first.html",
                    base + "deep/%E7%AC%AC%E4%BA%8C%E7%AF%87.html", base + "tags/", base + "categories/",
                    base + "categories/%E5%88%86%E7%B1%BB/", base + "tags/C%2B%2B/page/2/"}
        expected.update(base + "tags/" + quote(name, safe="") + "/" for name in tags)
        self.assertEqual(set(urls), expected)
        self.assertEqual(len(urls), len(expected))
        self.assertEqual(len(ET.parse(self.output / "rss.xml").findall("channel/item")), 2)

    def test_invalid_metadata_preserves_previous_output(self):
        self.build()
        target = self.content / "posts/first.md"
        invalid = ['"one"', '42', 'false', '{}', '[1]', '[true]', '[["nested"]]', '[{}]',
                   '[2026-11-22]', '[""]', '["   "]', '[".."]', '["."]', '["a/b"]',
                   '["a\\\\b"]', '["CON"]', '["nul.txt"]', '["COM1"]', '["name."]',
                   '["a\\nb"]', '["a\\u0000b"]', '["a?b"]', '["<tag>"]']
        for field in ("tags", "categories"):
            for value in invalid:
                with self.subTest(field=field, value=value):
                    target.write_text(f'+++\ntitle = "Bad"\ndate = "2026-11-22"\n{field} = {value}\n+++\nBody.',
                                      encoding="utf-8")
                    self.assert_failure_preserves("front matter", field)
        target.write_text('+++\ntitle = "Good"\ndate = "2026-11-22"\ntags = []\ncategories = []\n+++\nBody.',
                          encoding="utf-8")
        self.build()

    def test_independent_pages_do_not_silently_create_article_taxonomies(self):
        self.build()
        for field in ("tags", "categories"):
            self.document("pages/about.md", date=None, **{field: ["Not an article"]})
            self.assert_failure_preserves("only supported on articles", "pages/about.md")

    def test_literal_front_matter_arrays_and_duplicate_keys(self):
        self.probe_theme()
        self.write_page("posts/first.md", "Literal", "Body.")
        target = self.content / "posts/first.md"
        original = target.read_text(encoding="utf-8")
        target.write_text(original.replace('draft: false', 'draft: false\ntags: ["C++", "中文"]\ncategories: ["Notes"]'),
                          encoding="utf-8")
        self.build()
        self.assertEqual([e["data-name"] for e in self.page("posts/first.html").scope("page-tags")],
                         ["C++", "中文"])
        for field in ("tags", "categories"):
            target.write_text(original.replace('draft: false', f'{field}: ["One"]\n{field}: ["Two"]'),
                              encoding="utf-8")
            self.assert_failure_preserves("Duplicate front matter key: " + field)
            for value in ("# no value", "", '"one"', "[1]", '["../escape"]'):
                with self.subTest(field=field, value=value):
                    target.write_text(original.replace('draft: false', f'{field}: {value}'),
                                      encoding="utf-8")
                    self.assert_failure_preserves(field)


    def test_templates_required_only_for_populated_taxonomies_and_failure_rolls_back(self):
        overview = self.theme / "layouts/taxonomy.html"
        term = self.theme / "layouts/term.html"
        overview_source, term_source = overview.read_text(), term.read_text()
        overview.unlink()
        term.unlink()
        self.build()
        self.assertFalse((self.output / "tags").exists())
        self.document("posts/first.md", tags=["C++"])
        self.assert_failure_preserves("taxonomy.html")
        overview.write_text(overview_source)
        self.assert_failure_preserves("term.html")
        term.write_text('{{ undefined_term_value }}')
        self.assert_failure_preserves("undefined_term_value")
        term.write_text(term_source)
        self.build()
        self.assertTrue((self.output / "tags/C++/index.html").is_file())

    def test_generated_output_collisions_are_reported_with_both_sources(self):
        self.document("posts/first.md", tags=["C++"], categories=["Notes"])
        self.config('paginate_by = 1')
        self.document("posts/second.md", tags=["C++"])
        self.build()
        for name in ("tags/index.html", "TAGS/INDEX.HTML", "tags/C++/index.md",
                     "tags/C++/page/2/index.html", "categories/Notes/index.html",
                     "tags/index.html/child", "tags/C++"):
            with self.subTest(name=name):
                target = self.content / name
                target.parent.mkdir(parents=True, exist_ok=True)
                if name.endswith(".md"):
                    self.document(name)
                else:
                    target.write_text("Conflict", encoding="utf-8")
                self.assert_failure_preserves("Output path conflict", "generated", target.as_posix())
                target.unlink()
                for parent in target.parents:
                    if parent == self.content or any(parent.iterdir()):
                        break
                    parent.rmdir()
        self.document("posts/second.md", tags=["c++"])
        self.assert_failure_preserves("Output path conflict", "term 'C++'", "term 'c++'")

    def test_renames_drafts_deletions_and_pagination_remove_old_outputs(self):
        self.config('paginate_by = 1')
        self.document("posts/first.md", tags=["Old"], categories=["Notes"])
        self.document("posts/second.md", tags=["Old"], categories=["Notes"])
        self.build()
        self.assertTrue((self.output / "tags/Old/page/2/index.html").is_file())
        self.document("posts/second.md", tags=["New"], categories=["Notes"])
        self.build()
        self.assertFalse((self.output / "tags/Old/page").exists())
        self.document("posts/first.md", tags=["Old"], categories=["Notes"], draft=True)
        self.build()
        self.assertFalse((self.output / "tags/Old").exists())
        (self.content / "posts/second.md").unlink()
        self.build()
        self.assertFalse((self.output / "tags").exists())
        self.assertFalse((self.output / "categories").exists())
        self.assertNotIn('href="/tags/"', self.read())

    def test_bundled_themes_render_taxonomies_and_customization(self):
        self.config('paginate_by = 2\nbase_url = "https://example.test/blog/"')
        self.populate()
        for theme in (PROJECT_ROOT / "themes/default", PROJECT_ROOT / "examples/minimal-theme",
                      PROJECT_ROOT / "docs/theme"):
            with self.subTest(theme=theme):
                shutil.rmtree(self.theme)
                shutil.copytree(theme, self.theme)
                self.build()
                self.assertIn("C++", self.read("tags/index.html"))
                self.assertIn("First", self.read("tags/C++/index.html"))
                self.assertIn("page/2/", self.read("tags/C++/index.html"))
                self.assert_links_exist()
                layout = self.theme / "layouts/term.html"
                layout.write_text(layout.read_text(encoding="utf-8").replace('{% block content %}', '{% block content %}\n<p>Custom {{ taxonomy.name }} {{ term.name }}</p>', 1),
                                  encoding="utf-8")
                self.build()
                self.assertIn("Custom tags C++", self.read("tags/C++/index.html"))

    def test_embedded_default_theme_supports_taxonomies_outside_repository(self):
        target = self.root / "new-site"
        result = subprocess.run([str(self.binary), "init", str(target)], capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        article = target / "content/posts/welcome.md"
        article.write_text(article.read_text(encoding="utf-8").replace('title = ', 'tags = ["C++"]\ncategories = ["Notes"]\ntitle = ', 1),
                           encoding="utf-8")
        result = subprocess.run([str(self.binary), "build"], cwd=target, capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertTrue((target / "dist/tags/C++/index.html").is_file())
        self.assertTrue((target / "dist/categories/Notes/index.html").is_file())


if __name__ == "__main__":
    run_tests()
