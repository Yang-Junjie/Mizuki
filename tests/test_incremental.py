"""Verify cached rendering against complete transactional builds."""

import os

from support import BuilderFixture, run_tests


class IncrementalTests(BuilderFixture):
    def configure(self, enabled=True):
        self.set_config(f"[build]\nincremental = {str(enabled).lower()}\n")

    def set_config(self, settings):
        (self.root / "mizuki.toml").write_text(
            'base_url = "https://example.test/blog/"\narchives = true\npaginate_by = 1\n'
            + settings, encoding="utf-8"
        )

    def assert_matches_full_build(self):
        incremental = self.snapshot()
        self.configure(False)
        self.build()
        self.assertEqual(self.snapshot(), incremental)
        self.configure()

    def test_body_metadata_and_shared_template_changes(self):
        self.write_page("posts/second.md", "Second post", "Second body.", date="2026-11-17")
        self.configure()
        self.assertIn("2 rendered, 0 reused (Markdown); 5 rendered, 0 reused (HTML); published",
                      self.build())
        homepage = self.output / "index.html"
        os.utime(homepage, (1_700_000_000, 1_700_000_000))
        timestamp = homepage.stat().st_mtime_ns
        self.assertIn("0 rendered, 2 reused (Markdown); 0 rendered, 5 reused (HTML); output unchanged",
                      self.build())
        self.assertEqual(homepage.stat().st_mtime_ns, timestamp)

        self.write_page("posts/first.md", "First post", "## Heading\n\nEdited body.")
        self.assertIn("1 rendered, 1 reused (Markdown); 1 rendered, 4 reused (HTML); published",
                      self.build())
        article = (self.output / "posts/first.html").read_text(encoding="utf-8")
        self.assertIn('id="heading"', article)
        self.assertIn("Edited body.", article)
        self.assert_matches_full_build()

        self.write_page("posts/first.md", "Renamed post", "## Heading\n\nEdited body.")
        self.assertIn("0 rendered, 2 reused (Markdown); 5 rendered, 0 reused (HTML); published",
                      self.build())
        self.assertIn("Renamed post", (self.output / "archives/index.html").read_text(encoding="utf-8"))
        self.assert_matches_full_build()

        base = self.theme / "layouts/base.html"
        base.write_text(base.read_text(encoding="utf-8").replace("</body>", "<p>New footer</p></body>"),
                        encoding="utf-8")
        self.assertIn("0 rendered, 2 reused (Markdown); 5 rendered, 0 reused (HTML); published",
                      self.build())
        self.assertIn("New footer", homepage.read_text(encoding="utf-8"))
        self.assert_matches_full_build()

    def test_deleted_article_invalidates_links_and_stale_output(self):
        self.configure()
        self.write_page("posts/second.md", "Second post", "Second body.")
        self.write_page("posts/first.md", "First post", "[Second](second.md).")
        self.build()
        before = self.snapshot()
        (self.content / "posts/second.md").unlink()
        self.assertIn("missing or unpublished", self.build(success=False))
        self.assertEqual(self.snapshot(), before)

        self.write_page("posts/first.md", "First post", "No link.")
        self.build()
        self.assertFalse((self.output / "posts/second.html").exists())
        self.assertFalse((self.output / "page/2/index.html").exists())
        self.assert_matches_full_build()

    def test_cache_and_output_damage_recover(self):
        self.configure()
        self.build()
        expected = self.snapshot()
        root = self.root / ".cache/mizuki/render"
        for entry in root.iterdir():
            entry.write_bytes(b"broken")

        output = self.output / "index.html"
        output.write_bytes(b"!" * output.stat().st_size)
        (self.output / "empty-directory").mkdir()
        self.assertIn("published", self.build())
        self.assertEqual(self.snapshot(), expected)
        self.assertFalse((self.output / "empty-directory").exists())
        self.assertIn("output unchanged", self.build())

    def test_invalid_config_preserves_previous_site(self):
        self.build()
        before = self.snapshot()
        self.set_config('[build]\nincremental = "true"\n')
        self.assertIn("'build.incremental' must be a boolean", self.build(success=False))
        self.assertEqual(self.snapshot(), before)


if __name__ == "__main__":
    run_tests()
