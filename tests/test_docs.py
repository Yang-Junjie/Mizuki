"""Build the real documentation and check its deployed links without network access."""

from html.parser import HTMLParser
from pathlib import Path
from urllib.parse import unquote, urljoin, urlsplit
import json
import shutil
import subprocess
import tempfile
import unittest
import xml.etree.ElementTree as ET

from support import PROJECT_ROOT, binary_path, run_tests


class Page(HTMLParser):
    def __init__(self, source):
        super().__init__(convert_charrefs=True)
        self.ids = []
        self.links = []
        self.guide_links = []
        self.current = []
        self.canonical = []
        self.text = []
        self.feed(source)
        self.close()

    def handle_starttag(self, tag, attributes):
        attributes = dict(attributes)
        if "id" in attributes:
            self.ids.append(attributes["id"])
        if tag == "a" and attributes.get("class") == "guide-card":
            self.guide_links.append(attributes["href"])
        if attributes.get("aria-current") == "page":
            self.current.append(attributes.get("href"))
        if tag == "link" and attributes.get("rel") == "canonical":
            self.canonical.append(attributes.get("href"))
        for name in ("href", "src"):
            if name in attributes:
                self.links.append(attributes[name])

    def handle_data(self, data):
        self.text.append(data)


class DocumentationTest(unittest.TestCase):
    expected_pages = {
        "index.html",
        "pages/getting-started.html",
        "pages/themes.html",
        "pages/writing.html",
        "pages/architecture.html",
        "pages/deployment.html",
    }

    def setUp(self):
        self.binary = binary_path()
        temporary = tempfile.TemporaryDirectory(prefix="mizuki-docs-")
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)

    def build(self, base_url, *, config=None):
        output = self.root / "site"
        result = subprocess.run(
            [str(self.binary), "build", "--config", str(config or PROJECT_ROOT / "docs/mizuki.toml"),
             "--base-url", base_url, "--output", str(output)],
            cwd=self.root, capture_output=True, text=True, encoding="utf-8", timeout=30,
        )
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        return output

    def check_site(self, output, base_url):
        pages = {
            path.relative_to(output).as_posix(): Page(path.read_text(encoding="utf-8"))
            for path in output.rglob("*.html")
        }
        self.assertEqual(set(pages), self.expected_pages)
        self.assertEqual(pages["index.html"].guide_links,
                         sorted(self.expected_pages - {"index.html"}))
        origin = urlsplit(base_url)
        prefix = unquote(origin.path)
        expected_canonicals = set()
        for name, page in pages.items():
            current_url = urljoin(base_url, name)
            canonical = base_url if name == "index.html" else current_url
            expected_canonicals.add(canonical)
            self.assertEqual(page.canonical, [canonical], name)
            self.assertEqual(len(page.current), 1, name)
            self.assertEqual(len(page.ids), len(set(page.ids)), name)
            self.assertIn("main", page.ids)
            self.assertEqual(urljoin(current_url, page.current[0]), current_url, name)
            for link in page.links:
                resolved = urlsplit(urljoin(current_url, link))
                if (resolved.scheme, resolved.netloc) != (origin.scheme, origin.netloc):
                    continue
                deployed_path = unquote(resolved.path)
                self.assertTrue(deployed_path.startswith(prefix), (name, link))
                local = deployed_path[len(prefix):]
                if not local or local.endswith("/"):
                    local += "index.html"
                target = output / local
                self.assertTrue(target.is_file(), (name, link, local))
                if resolved.fragment and local in pages:
                    self.assertIn(unquote(resolved.fragment), pages[local].ids, (name, link))
            source = (output / name).read_text(encoding="utf-8")
            self.assertNotIn("EventSource(", source)
            self.assertNotIn("__mizuki/events", source)
        sitemap = ET.parse(output / "sitemap.xml")
        urls = {node.text for node in sitemap.findall("{*}url/{*}loc")}
        self.assertEqual(urls, expected_canonicals)
        self.assertEqual(ET.parse(output / "rss.xml").findall("channel/item"), [])
        self.assertTrue((output / "assets/css/docs.css").is_file())
        self.assertEqual(
            {path.relative_to(output).as_posix() for path in output.rglob("*") if path.is_file()},
            self.expected_pages | {"rss.xml", "sitemap.xml"} | {
                "assets/" + path.relative_to(PROJECT_ROOT / "docs/theme/assets").as_posix()
                for path in (PROJECT_ROOT / "docs/theme/assets").rglob("*") if path.is_file()
            },
        )
        theme_text = "".join(pages["pages/themes.html"].text)
        self.assertIn("{{ html(page.content) }}", theme_text)
        self.assertIn('{% extends "base.html" %}', theme_text)
        self.assertIn("OutputTransaction", "".join(pages["pages/architecture.html"].text))

    def test_root_and_project_deployments(self):
        for base_url in ("https://docs.example.test/", "https://docs.example.test/Mizuki/",
                         "https://docs.example.test/%E6%96%87%E6%A1%A3%20%E7%AB%99/"):
            with self.subTest(base_url=base_url):
                self.check_site(self.build(base_url), base_url)

    def test_new_documents_automatically_become_home_entries_without_navigation_changes(self):
        content = self.root / "content"
        shutil.copytree(PROJECT_ROOT / "docs/content", content)
        target = content / "pages/deep/新增 guide.md"
        target.parent.mkdir()
        target.write_text("+++\ntitle = '<New & guide>'\n"
                          'description = "Own description <&>"\n+++\n\n'
                          '[Themes](../themes.md)\n', encoding="utf-8")
        (content / "pages/draft.md").write_text('+++\ntitle = "Hidden guide"\n'
                                               'draft = true\n+++\nDraft.', encoding="utf-8")
        config = self.root / "docs.toml"
        original = (PROJECT_ROOT / "docs/mizuki.toml").read_text(encoding="utf-8")
        config.write_text(original.replace('content = "content"',
                                           'content = ' + json.dumps(str(content)))
                          .replace('theme = "theme"', 'theme = ' + json.dumps(
                              str(PROJECT_ROOT / "docs/theme"))), encoding="utf-8")
        output = self.build("https://docs.example.test/Mizuki/", config=config)
        homepage = (output / "index.html").read_text(encoding="utf-8")
        self.assertIn('pages/deep/%E6%96%B0%E5%A2%9E%20guide.html', Page(homepage).guide_links)
        self.assertIn("&lt;New &amp; guide&gt;", homepage)
        self.assertIn("Own description &lt;&amp;&gt;", homepage)
        self.assertNotIn("Hidden guide", homepage)
        # A card description remains overridable without repeating all page metadata in the theme.
        with config.open("a", encoding="utf-8") as stream:
            stream.write('\n[extra.guide_descriptions]\n"<New & guide>" = "Override <&>"\n')
        homepage = (self.build("/Mizuki/", config=config) / "index.html").read_text(encoding="utf-8")
        self.assertIn("Override &lt;&amp;&gt;", homepage)
        self.assertNotIn("Own description &lt;&amp;&gt;", homepage)

    def test_local_preview_build(self):
        output = self.build("/Mizuki/")
        self.assertFalse((output / "rss.xml").exists())
        self.assertFalse((output / "sitemap.xml").exists())
        self.assertTrue((output / "index.html").is_file())
        self.assertIn('href="/Mizuki/"', (output / "index.html").read_text(encoding="utf-8"))
        for path in output.rglob("*.html"):
            self.assertEqual(Page(path.read_text(encoding="utf-8")).canonical, [])


if __name__ == "__main__":
    run_tests()
