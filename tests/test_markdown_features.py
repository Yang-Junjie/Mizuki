"""Technical writing markup, conditional assets and the public theme contract."""

from html.parser import HTMLParser
from pathlib import Path
import re

from support import BuilderFixture, DEFAULT_THEME, PROJECT_ROOT, run_tests


class Markup(HTMLParser):
    def __init__(self, source):
        super().__init__()
        self.tags = []
        self.feed(source)

    def handle_starttag(self, tag, attrs):
        self.tags.append((tag, dict(attrs)))


class MarkdownFeaturesTest(BuilderFixture):
    def config(self, value):
        (self.root / "mizuki.toml").write_text(value, encoding="utf-8")

    def page(self, name="posts/first.html"):
        return (self.output / name).read_text(encoding="utf-8")

    def expose_features(self):
        template = self.theme / "layouts/base.html"
        template.write_text(template.read_text(encoding="utf-8") + '''
<!--features:{{ page.features.code }},{{ page.features.math }},{{ page.features.footnotes }},{{ page.features.admonitions }}-->
<!--languages:{% for language in page.code_languages %}[{{ language }}]{% endfor %}-->
''', encoding="utf-8")

    def test_markup_features_and_deeply_nested_asset_paths(self):
        self.expose_features()
        self.config('[markdown]\nmath = true\n')
        self.write_page("pages/guides/writing.md", "Writing", r'''
## Formula $a < b$

Inline $e^{i\pi}+1=0$ and display:

$$
\sum_{n=1}^{\infty} \frac{1}{n^2} = \frac{\pi^2}{6}
$$

A reference[^note], repeated[^note].

[^note]: A footnote with **emphasis** and [a link](../../posts/first.md).

> [!WARNING]
> Save a backup.

```cpp
const auto text = "<script> & $not_math$";
```

```toml
name = "Mizuki"
```

```cpp
int answer = 42;
```
''')
        self.build()
        source = self.page("pages/guides/writing.html")
        self.assertIn("<!--features:true,true,true,true-->", source)
        self.assertIn("<!--languages:[cpp][toml]-->", source)
        self.assertIn('<x-equation>a &lt; b</x-equation>', source)
        self.assertIn('<x-equation type="display">', source)
        self.assertIn('class="language-cpp"', source)
        self.assertIn('&lt;script&gt; &amp; $not_math$', source)
        self.assertIn('class="admonition-warning"', source)
        self.assertIn('<strong>emphasis</strong>', source)
        markup = Markup(source)
        ids = [attrs['id'] for _, attrs in markup.tags if 'id' in attrs]
        self.assertEqual(len(ids), len(set(ids)))
        backrefs = [attrs['href'] for tag, attrs in markup.tags
                    if tag == 'a' and attrs.get('class') == 'footnote-backref']
        self.assertEqual(backrefs, ['#fnref-1-1', '#fnref-1-2'])
        self.assertTrue(all(href[1:] in ids for href in backrefs))
        self.assertIn('href="../../posts/first.html"', source)
        assets = [attrs.get('src', attrs.get('href')) for tag, attrs in markup.tags
                  if tag in ('link', 'script') and ('src' in attrs or 'href' in attrs)]
        for asset in assets:
            self.assertTrue((self.output / 'pages/guides' / asset).resolve().is_file(), asset)
        self.assertIn('../../assets/vendor/katex/katex.min.js', assets)
        self.assertIn('../../assets/vendor/highlight/highlight.min.js', assets)
        self.assertNotIn('vendor/katex', self.page('index.html'))
        self.assertNotIn('js/code.js', self.page())
        self.assertIn('<!--features:false,false,false,false-->', self.page('index.html'))

    def test_math_is_opt_in_and_ignores_code_and_escaped_dollars(self):
        self.expose_features()
        self.write_page('posts/first.md', 'Money', r'Cost $5 and $10. Formula $a+b$.')
        self.build()
        self.assertNotIn('<x-equation', self.page())
        self.assertNotIn('vendor/katex', self.page())
        self.config('[markdown]\nmath = true\n')
        self.write_page('posts/first.md', 'Code', r'''
Escaped \$5 and \$10; one unmatched $5.

`$inline$`

```
$$block$$
```
''')
        self.build()
        source = self.page()
        self.assertNotIn('<x-equation', source)
        self.assertIn('<code>$inline$</code>', source)
        self.assertIn('<!--features:true,false,false,false-->', source)
        self.assertNotIn('vendor/katex', source)

    def test_independent_syntax_and_theme_switches(self):
        self.expose_features()
        self.config('''[markdown]
math = true
footnotes = false
admonitions = false
[extra]
code_highlight = false
code_copy = false
math_render = false
''')
        self.write_page('posts/first.md', 'Disabled', '''$x$ [^note]

[^note]: Definition.

> [!NOTE]
> Plain quote.

```python
print("Hello")
```
''')
        self.build()
        source = self.page()
        self.assertIn('<x-equation>x</x-equation>', source)
        self.assertIn('<!--features:true,true,false,false-->', source)
        self.assertNotIn('class="footnotes"', source)
        self.assertNotIn('class="admonition-note"', source)
        self.assertNotIn('<script', source)
        self.assertNotIn('vendor/katex', source)
        self.assertIn('css/reading.css', source)
        self.config('[extra]\ncode_highlight = false\ncode_copy = true\n')
        self.build()
        self.assertNotIn('vendor/highlight', self.page())
        self.assertIn('data-highlight="false" data-copy="true"', self.page())

    def test_reading_parameters_have_fallback_defaults(self):
        (self.theme / 'theme.toml').unlink()
        self.config('[markdown]\nmath = true\n[extra]\nshow_toc = true\nfooter_text = ""\n')
        self.write_page('posts/first.md', 'Defaults', '$x$\n\n```cpp\nint x = 1;\n```')
        self.build()
        self.assertIn('vendor/highlight/highlight.min.js', self.page())
        self.assertIn('data-highlight="true" data-copy="true"', self.page())
        self.assertIn('vendor/katex/katex.min.js', self.page())

    def test_raw_html_comments_and_script_text_do_not_report_false_features(self):
        self.expose_features()
        self.write_page('posts/first.md', 'Raw', '''
<!-- <pre><code class="language-python">fake</code></pre><x-equation>x</x-equation> -->
<script>const example = '<pre><code class="language-js">fake</code></pre>';</script>
<style>/* <x-equation>x</x-equation> */</style>

`<x-equation>fake</x-equation>`
''')
        self.build()
        self.assertIn('<!--features:false,false,false,false-->', self.page())
        self.write_page('posts/first.md', 'Raw', '''
<pre><code class="other language-rust">fn main() {}</code></pre>
<x-equation type="display">x^2</x-equation>
''')
        self.build()
        self.assertIn('<!--features:true,true,false,false-->', self.page())
        self.assertIn('<!--languages:[rust]-->', self.page())

    def test_strict_markdown_configuration_preserves_previous_output(self):
        self.build()
        before = self.snapshot()
        for value, message in [
            ('markdown = true', "'markdown' must be a table"),
            ('[markdown]\nmath = "true"', "'markdown.math' must be a boolean"),
            ('[markdown]\nfootnotes = 1', "'markdown.footnotes' must be a boolean"),
            ('[markdown]\nadmonitions = []', "'markdown.admonitions' must be a boolean"),
            ('[markdown]\nhighlight = true', 'Unknown configuration key: highlight'),
        ]:
            with self.subTest(value=value):
                self.config(value)
                self.assertIn(message, self.build(success=False))
                self.assertEqual(self.snapshot(), before)

    def test_offline_assets_licenses_fonts_and_theme_copies(self):
        for relative in ['assets/js/code.js', 'assets/js/math.js', 'assets/css/reading.css',
                         'layouts/partials/reading.html']:
            self.assertEqual((DEFAULT_THEME / relative).read_bytes(),
                             (PROJECT_ROOT / 'docs/theme' / relative).read_bytes(), relative)
        vendor = DEFAULT_THEME / 'assets/vendor'
        files = {p.relative_to(vendor) for p in vendor.rglob('*') if p.is_file()}
        other = PROJECT_ROOT / 'docs/theme/assets/vendor'
        self.assertEqual(files, {p.relative_to(other) for p in other.rglob('*') if p.is_file()})
        for relative in files:
            self.assertEqual((vendor / relative).read_bytes(), (other / relative).read_bytes())
        for package in ('highlight', 'katex'):
            self.assertGreater((vendor / package / 'LICENSE').stat().st_size, 500)
        katex = vendor / 'katex'
        css = (katex / 'katex.min.css').read_text(encoding='utf-8')
        fonts = re.findall(r'url\(([^)]+)\)', css)
        self.assertTrue(fonts)
        self.assertEqual(set(fonts), {p.relative_to(katex).as_posix() for p in (katex / 'fonts').glob('*.woff2')})
        self.assertTrue(all((katex / font).is_file() and font.endswith('.woff2') for font in fonts))
        self.assertNotIn('https://', css)


if __name__ == '__main__':
    run_tests()
