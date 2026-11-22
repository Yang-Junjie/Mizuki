"""Heading permalinks, static tables of contents and links to published sections."""

from html.parser import HTMLParser
from urllib.parse import unquote, urlsplit

from support import BuilderFixture, run_tests


class ReadingMarkup(HTMLParser):
    def __init__(self, source):
        super().__init__()
        self.headings = []
        self.permalinks = []
        self.toc_links = []
        self.links = []
        self.ids = []
        self.list_errors = []
        self.toc = False
        self.list_stack = []
        self.current_heading = None
        self.current_toc_link = None
        self.permalink = False
        self.feed(source)

    def handle_starttag(self, tag, attrs):
        attrs = dict(attrs)
        if "id" in attrs:
            self.ids.append(attrs["id"])
        if tag == "nav" and attrs.get("class") == "toc":
            self.toc = True
        if tag in ("h1", "h2", "h3", "h4", "h5", "h6") and "id" in attrs:
            self.current_heading = [tag, attrs["id"], ""]
        if self.toc and tag in ("ol", "li"):
            expected_parent = "li" if tag == "ol" else "ol"
            if self.list_stack and self.list_stack[-1] != expected_parent:
                self.list_errors.append((tag, self.list_stack.copy()))
            self.list_stack.append(tag)
        if tag == "a":
            self.links.append(attrs.get("href", ""))
            if attrs.get("class") == "heading-anchor":
                self.permalinks.append(attrs)
                self.permalink = True
            if self.toc:
                self.current_toc_link = [attrs.get("href"), self.list_stack.count("ol"), ""]
        if self.current_heading and tag == "img":
            self.current_heading[2] += attrs.get("alt", "")

    def handle_endtag(self, tag):
        if tag in ("h1", "h2", "h3", "h4", "h5", "h6") and self.current_heading:
            self.current_heading[2] = self.current_heading[2].strip()
            self.headings.append(tuple(self.current_heading))
            self.current_heading = None
        if tag == "a":
            self.permalink = False
            if self.current_toc_link:
                self.toc_links.append(tuple(self.current_toc_link))
                self.current_toc_link = None
        if self.toc and tag in ("ol", "li"):
            if not self.list_stack or self.list_stack.pop() != tag:
                self.list_errors.append(("closing", tag))
        if tag == "nav" and self.toc:
            if self.list_stack:
                self.list_errors.append(("unclosed", self.list_stack.copy()))
            self.toc = False

    def handle_data(self, data):
        if self.current_heading and not self.permalink:
            self.current_heading[2] += data
        if self.current_toc_link:
            self.current_toc_link[2] += data


class ReadingTests(BuilderFixture):
    def read(self, name="posts/first.html"):
        return (self.output / name).read_text(encoding="utf-8")

    def render(self, body):
        self.write_page("posts/first.md", "Reading", body)
        self.build()
        self.html = self.read()
        return ReadingMarkup(self.html)

    def assert_links_reach_headings(self, markup):
        ids = {identifier for _, identifier, _ in markup.headings}
        for link in markup.permalinks:
            self.assertIn(unquote(urlsplit(link["href"]).fragment), ids)
            self.assertTrue(link["aria-label"])
        for href, _, _ in markup.toc_links:
            self.assertIn(unquote(urlsplit(href).fragment), ids)
        self.assertEqual(markup.list_errors, [])

    def test_chinese_duplicate_formatted_setext_and_empty_headings(self):
        markup = self.render("""# 总览

## 安装

## 安装

### *配置* 与 `C++`

English Heading
--------------

###### 最后

## !!!

##
""")
        self.assertEqual([row[:2] for row in markup.headings], [
            ("h1", "总览"), ("h2", "安装"), ("h2", "安装-2"), ("h3", "配置-与-c"),
            ("h2", "english-heading"), ("h6", "最后"), ("h2", "section"), ("h2", "section-2"),
        ])
        self.assertIn('<em>配置</em> 与 <code>C++</code>', self.html)
        self.assertEqual(len(markup.permalinks), 8)
        self.assertEqual(len(markup.toc_links), 6)
        self.assertEqual(markup.toc_links[-1][2], "（无标题）")
        self.assert_links_reach_headings(markup)
        before = self.snapshot()
        self.build()
        self.assertEqual(self.snapshot(), before)

    def test_entities_labels_images_and_ascii_punctuation(self):
        markup = self.render("""## A &amp; B &lt;tag&gt; &quot;Q&quot; &eacute; &#20013;

## [链接](https://example.com/) ![图示](image.png)

## Foo&nbsp;Bar　BAZ

## 标点：保留！

## `<script>` &bogus;

<h2>Authored & *literal*;</h2>
""")
        self.assertEqual([row[1] for row in markup.headings], [
            "a-b-tag-q-é-中", "链接-图示", "foo-bar-baz", "标点：保留！", "script-bogus",
            "authored-literal",
        ])
        self.assertEqual(markup.toc_links[0][2], 'A & B <tag> "Q" é 中')
        self.assertEqual(markup.toc_links[1][2], "链接 图示")
        self.assertIn("https://example.com/", markup.links)
        self.assertNotIn("<script>", self.html)
        self.assertEqual(markup.toc_links[-2][2], "<script> &bogus;")
        self.assertEqual(markup.toc_links[-1][2], "Authored & *literal*;")
        self.assert_links_reach_headings(markup)

    def test_skipped_levels_and_orphan_subheadings_make_valid_lists(self):
        markup = self.render("""### First orphan

### Second orphan

## Parent

#### Unlisted

### Child

### Another child

# New section

### Third orphan

## Next parent

### Last child
""")
        self.assertEqual([(text, depth) for _, depth, text in markup.toc_links], [
            ("First orphan", 1), ("Second orphan", 1), ("Parent", 1), ("Child", 2),
            ("Another child", 2), ("Third orphan", 1), ("Next parent", 1), ("Last child", 2),
        ])
        self.assert_links_reach_headings(markup)

    def test_code_comments_and_raw_text_do_not_create_headings(self):
        body = """```html
<h2>Fenced heading</h2>
## Fenced Markdown
```

    ## Indented heading

<!-- <h2>Comment heading</h2> -->

<script>const example = "<h2>Script heading</h2>";</script>

<style>/* <h2>Style heading</h2> */</style>

<textarea><h2>Textarea heading</h2></textarea>

<pre><code><h2>Raw code heading</h2></code></pre>

## Real `&lt;h2&gt;` heading
"""
        markup = self.render(body)
        self.assertEqual(len(markup.permalinks), 1)
        self.assertEqual([text for _, _, text in markup.toc_links], ["Real &lt;h2&gt; heading"])
        for raw in ('<!-- <h2>Comment heading</h2> -->',
                    '<script>const example = "<h2>Script heading</h2>";</script>',
                    '<style>/* <h2>Style heading</h2> */</style>',
                    '<textarea><h2>Textarea heading</h2></textarea>',
                    '<pre><code><h2>Raw code heading</h2></code></pre>'):
            self.assertIn(raw, self.html)

    def test_authored_ids_and_natural_suffixes_do_not_collide(self):
        markup = self.render("""## Install

## Install

## Install-3

<div id="install">Reserved later in the document.</div>

<h2 ID='install-2' data-note='a > b'>Authored &eacute; heading</h2>

<h2 id="">Empty ID</h2>

<h2 id>Boolean empty ID</h2>

## Install
""")
        self.assertEqual([row[1] for row in markup.headings], [
            "install-3", "install-4", "install-3-2", "install-2", "empty-id", "boolean-empty-id",
            "install-5",
        ])
        self.assertEqual(len(markup.ids), len(set(markup.ids)))
        self.assertIn("<h2 ID='install-2' data-note='a > b'>", self.html)
        self.assertEqual(markup.toc_links[3][2], "Authored é heading")
        self.assert_links_reach_headings(markup)

    def test_no_listed_headings_produces_no_directory(self):
        for body in ("Plain paragraph.", "# Only top level\n\n#### Deep section"):
            with self.subTest(body=body):
                markup = self.render(body)
                self.assertEqual(markup.toc_links, [])
                self.assertNotIn('class="toc"', self.html)
                self.assertNotIn('{% include "partials/toc.html" %}', self.html)
                self.assert_links_reach_headings(markup)

    def test_optional_placeholder_and_independent_pages(self):
        (self.content / "pages").mkdir()
        (self.content / "pages/about.md").write_text(
            "---\ntitle: About\n---\n\n## About section\n", encoding="utf-8")
        self.render("## Article section")
        markup = ReadingMarkup(self.read("pages/about.html"))
        self.assertEqual(markup.toc_links, [("#about-section", 1, "About section")])
        for name in ("post.html", "page.html"):
            path = self.theme / "layouts" / name
            path.write_text(path.read_text(encoding="utf-8").replace('{% include "partials/toc.html" %}', ""), encoding="utf-8")
        self.build()
        for name in ("posts/first.html", "pages/about.html"):
            markup = ReadingMarkup(self.read(name))
            self.assertEqual(markup.toc_links, [])
            self.assertEqual(len(markup.permalinks), 1)

    def test_subpath_and_cross_document_fragments_reach_published_sections(self):
        (self.root / "mizuki.toml").write_text('base_url = "/blog/"\n', encoding="utf-8")
        self.write_page("pages/about.md", "About", "## 安装\n\n## 安装")
        markup = self.render("""## 开始

[相对路径](../pages/about.md?from=article#安装)

[站点根路径](/pages/about.md#安装-2)

[本页](#开始)
""")
        self.assert_links_reach_headings(markup)
        target = ReadingMarkup(self.read("pages/about.html"))
        target_ids = {identifier for _, identifier, _ in target.headings}
        links = [unquote(link) for link in markup.links]
        self.assertIn("../pages/about.html?from=article#安装", links)
        self.assertIn("/blog/pages/about.html#安装-2", links)
        for link in links:
            if "pages/about.html" in link:
                self.assertIn(urlsplit(link).fragment, target_ids)

    def test_static_html_is_copied_without_heading_changes(self):
        original = '<h2 id="own">Static heading</h2><h3>Unchanged</h3>'
        (self.content / "raw.html").write_text(original, encoding="utf-8")
        self.build()
        self.assertEqual(self.read("raw.html"), original)


if __name__ == "__main__":
    run_tests()
