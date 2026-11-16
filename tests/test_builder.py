"""CLI regressions for complete builds and transactional output replacement."""

from pathlib import Path
import os
import shutil
import subprocess
import sys
import tempfile
import unittest


BINARY = Path(sys.argv.pop(1)).resolve()
DEFAULT_THEME = Path(__file__).resolve().parents[1] / "themes" / "default"


class BuilderTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="mizuki-builder-")
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.content = self.root / "content"
        self.theme = self.root / "theme"
        self.output = self.root / "dist"
        self.content.mkdir()
        shutil.copytree(DEFAULT_THEME, self.theme)
        self.write_page("index.md", "Home", "Welcome.")
        self.write_page("posts/first.md", "First post", "Original body.")

    def write_page(self, name, title, body, draft=False, date="2026-11-16"):
        path = self.content / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(
            f"---\ntitle: {title}\ndate: {date}\n"
            f"draft: {'true' if draft else 'false'}\n---\n\n{body}\n",
            encoding="utf-8",
        )

    def build(self, *, success=True, output=None, cwd=None):
        result = subprocess.run(
            [
                str(BINARY), "build",
                "--content", str(self.content),
                "--theme", str(self.theme),
                "--output", str(self.output if output is None else output),
            ],
            cwd=self.root if cwd is None else cwd,
            capture_output=True,
            text=True,
            timeout=15,
        )
        self.assertEqual(result.returncode, 0 if success else 1, result.stdout + result.stderr)
        return result.stdout + result.stderr

    def snapshot(self, directory=None):
        directory = self.output if directory is None else directory
        return {
            path.relative_to(directory).as_posix(): path.read_bytes()
            for path in directory.rglob("*")
            if path.is_file()
        }

    def assert_no_temporary_directories(self, output=None):
        output = self.output if output is None else Path(output)
        self.assertEqual(list(output.parent.glob(output.name + ".mizuki-*")), [])

    def make_symlink(self, link, target):
        try:
            link.symlink_to(target, target_is_directory=True)
        except (OSError, NotImplementedError) as error:
            self.skipTest(f"Directory symlinks unavailable: {error}")

    def test_initial_and_repeated_builds(self):
        self.build()
        self.assertEqual(
            set(self.snapshot()),
            {"index.html", "posts/first.html", "assets/css/style.css"},
        )
        self.assertIn("Original body.", (self.output / "posts/first.html").read_text())
        homepage = (self.output / "index.html").read_text()
        self.assertIn("First post", homepage)
        self.assertIn('href="posts/first.html"', homepage)
        self.assertNotIn("Welcome.", homepage)
        self.assertEqual(
            (self.output / "assets/css/style.css").read_bytes(),
            (self.theme / "assets/css/style.css").read_bytes(),
        )
        self.write_page("posts/first.md", "Updated title", "Updated body.")
        self.build()
        self.assertIn("Updated title", (self.output / "posts/first.html").read_text())
        self.assertIn("Updated body.", (self.output / "posts/first.html").read_text())
        self.assert_no_temporary_directories()

    def test_deleted_and_renamed_articles_disappear(self):
        self.build()
        (self.content / "posts/first.md").rename(self.content / "posts/renamed.md")
        self.build()
        self.assertFalse((self.output / "posts/first.html").exists())
        self.assertTrue((self.output / "posts/renamed.html").is_file())
        homepage = (self.output / "index.html").read_text()
        self.assertNotIn('href="posts/first.html"', homepage)
        self.assertIn('href="posts/renamed.html"', homepage)
        (self.content / "posts/renamed.md").unlink()
        self.build()
        self.assertFalse((self.output / "posts/renamed.html").exists())
        self.assertTrue((self.output / "index.html").is_file())
        self.assert_no_temporary_directories()

    def test_draft_transitions_remove_and_restore_output(self):
        self.build()
        self.write_page("posts/first.md", "First post", "Draft body.", draft=True)
        self.build()
        self.assertFalse((self.output / "posts/first.html").exists())
        self.assertNotIn("First post", (self.output / "index.html").read_text())
        self.write_page("posts/first.md", "First post", "Published again.")
        self.build()
        self.assertIn("Published again.", (self.output / "posts/first.html").read_text())
        self.assertIn("First post", (self.output / "index.html").read_text())
        self.assert_no_temporary_directories()

    def test_homepage_template_controls_layout_and_lists_articles_by_date(self):
        (self.theme / "layouts/home.html").write_text(
            '<!doctype html><title>Custom home</title><link href="{asset_base}site.css">'
            '<main><ul>{posts}</ul></main>',
            encoding="utf-8",
        )
        self.write_page(
            "posts/old.md", "Old & archived", "Old body.", date="2024-01-01 & old"
        )
        self.write_page(
            "posts/nested/latest.md", 'Latest <article> "new"', "Latest body.",
            date="2026-12-02",
        )
        self.write_page(
            "posts/nested/space #&.md", "Special path", "Special body.", date="2025-01-01"
        )
        self.write_page(
            "posts/draft.md", "Hidden draft", "Draft body.", draft=True, date="2030-01-01"
        )

        self.build()
        homepage = (self.output / "index.html").read_text()
        self.assertTrue(homepage.startswith("<!doctype html><title>Custom home</title>"))
        self.assertIn('href="posts/nested/latest.html"', homepage)
        self.assertIn('href="posts/nested/space%20%23%26.html"', homepage)
        self.assertIn("Latest &lt;article&gt; &quot;new&quot;", homepage)
        self.assertIn("Old &amp; archived", homepage)
        self.assertIn("2024-01-01 &amp; old", homepage)
        self.assertNotIn("Hidden draft", homepage)
        self.assertLess(
            homepage.index("Latest &lt;article&gt;"), homepage.index("First post")
        )
        self.assertLess(homepage.index("First post"), homepage.index("Special path"))
        self.assertLess(homepage.index("Special path"), homepage.index("Old &amp; archived"))
        self.assertEqual(homepage.count("<li class=\"post-item\">"), 4)

    def test_index_markdown_is_not_used_or_validated_for_homepage(self):
        (self.content / "index.md").write_text("Not valid front matter.", encoding="utf-8")
        self.build()
        homepage = (self.output / "index.html").read_text()
        self.assertIn("First post", homepage)
        self.assertNotIn("Not valid front matter", homepage)

    def test_homepage_with_no_published_articles_has_empty_list(self):
        (self.content / "posts/first.md").unlink()
        self.write_page("posts/draft.md", "Draft only", "Unpublished.", draft=True)
        self.build()
        homepage = (self.output / "index.html").read_text()
        self.assertIn('<ul class="post-list">\n      </ul>', homepage)
        self.assertNotIn("Draft only", homepage)

    def test_deleted_content_and_theme_assets_disappear(self):
        resource = self.content / "images" / "photo.bin"
        resource.parent.mkdir()
        resource.write_bytes(b"\x00\x01image\xff")
        self.build()
        self.assertEqual((self.output / "images/photo.bin").read_bytes(), resource.read_bytes())
        resource.unlink()
        (self.theme / "assets/css/style.css").unlink()
        self.build()
        self.assertFalse((self.output / "images/photo.bin").exists())
        self.assertFalse((self.output / "assets/css/style.css").exists())
        self.assert_no_temporary_directories()

    def test_empty_content_and_missing_theme_assets_clear_old_output(self):
        self.build()
        shutil.rmtree(self.content)
        self.content.mkdir()
        shutil.rmtree(self.theme / "assets")
        self.build()
        self.assertTrue(self.output.is_dir())
        self.assertEqual({path.name for path in self.output.iterdir()}, {"index.html"})
        self.assertIn("<ul class=\"post-list\">", (self.output / "index.html").read_text())
        self.assert_no_temporary_directories()

    def test_successful_build_removes_manual_files(self):
        self.build()
        (self.output / "manual.txt").write_text("Not managed separately.")
        (self.output / "manual-directory").mkdir()
        (self.output / "manual-directory/extra.txt").write_text("Extra file.")
        self.build()
        self.assertFalse((self.output / "manual.txt").exists())
        self.assertFalse((self.output / "manual-directory").exists())
        self.assert_no_temporary_directories()

    def test_invalid_front_matter_preserves_previous_output(self):
        self.build()
        (self.output / "manual.txt").write_text("Preserve even manual files on failure.")
        before = self.snapshot()
        self.write_page("index.md", "Changed home", "New content must not be published.")
        (self.content / "posts/first.md").write_text("Missing front matter.")
        self.build(success=False)
        self.assertEqual(self.snapshot(), before)
        self.assert_no_temporary_directories()

    def test_invalid_or_missing_template_preserves_previous_output(self):
        self.build()
        before = self.snapshot()
        layout = self.theme / "layouts/post.html"
        for invalid in ("<html>Missing placeholders</html>", None):
            with self.subTest(template=invalid):
                if invalid is None:
                    layout.unlink()
                else:
                    layout.write_text(invalid)
                self.build(success=False)
                self.assertEqual(self.snapshot(), before)
                self.assert_no_temporary_directories()

    def test_invalid_or_missing_homepage_template_preserves_previous_output(self):
        self.build()
        before = self.snapshot()
        layout = self.theme / "layouts/home.html"
        for invalid in ("<html>Missing placeholders</html>", None):
            with self.subTest(template=invalid):
                if invalid is None:
                    layout.unlink()
                else:
                    layout.write_text(invalid, encoding="utf-8")
                self.build(success=False)
                self.assertEqual(self.snapshot(), before)
                self.assert_no_temporary_directories()

    @unittest.skipUnless(os.name == "posix", "POSIX file permissions required")
    def test_failed_asset_copy_preserves_previous_output(self):
        resource = self.content / "image.bin"
        resource.write_bytes(b"original asset")
        self.build()
        before = self.snapshot()
        original_mode = resource.stat().st_mode
        try:
            resource.chmod(0)
            if os.access(resource, os.R_OK):
                self.skipTest("Current user can read files without read permissions")
            self.build(success=False)
        finally:
            resource.chmod(original_mode)
        self.assertEqual(self.snapshot(), before)
        self.assert_no_temporary_directories()

    @unittest.skipUnless(os.name == "posix", "POSIX directory permissions required")
    def test_backup_cleanup_failure_warns_but_publishes_new_site(self):
        self.build()
        locked = self.output / "locked"
        locked.mkdir()
        (locked / "manual.txt").write_text("Old site backup.")
        locked.chmod(0o500)
        try:
            if os.access(locked, os.W_OK):
                self.skipTest("Current user can write directories without write permissions")
            self.write_page("posts/first.md", "Updated post", "Published despite cleanup warning.")
            log = self.build()
            self.assertIn("Failed to remove build directory", log)
            self.assertIn(".mizuki-backup-", log)
            self.assertIn("Updated post", (self.output / "index.html").read_text())
            self.assertIn(
                "Published despite cleanup warning.",
                (self.output / "posts/first.html").read_text(),
            )
            self.assertFalse((self.output / "locked").exists())
            backups = list(self.root.glob("dist.mizuki-backup-*"))
            self.assertEqual(len(backups), 1)
            self.assertEqual(
                (backups[0] / "site/locked/manual.txt").read_text(), "Old site backup."
            )
        finally:
            # Restore permissions wherever the old directory ended up before fixture cleanup.
            for directory in [locked, *self.root.glob("dist.mizuki-backup-*/site/locked")]:
                if directory.exists():
                    directory.chmod(0o700)

    def test_first_failed_build_does_not_create_output(self):
        (self.content / "posts/first.md").write_text("Invalid article.")
        self.build(success=False)
        self.assertFalse(self.output.exists())
        self.assert_no_temporary_directories()

    def test_relative_nested_output_with_trailing_separator(self):
        output = self.root / "public/site"
        self.build(output="public/site" + os.sep)
        self.assertTrue((output / "index.html").is_file())
        self.build(output="public/site" + os.sep)
        self.assertTrue((output / "posts/first.html").is_file())
        self.assert_no_temporary_directories(output)

    def test_similarly_named_sibling_is_allowed(self):
        output = self.root / "content-export"
        self.build(output=output)
        self.assertTrue((output / "index.html").is_file())
        self.assert_no_temporary_directories(output)

    def test_empty_output_is_rejected_without_changes(self):
        before = self.snapshot(self.root)
        self.assertIn("must not be empty", self.build(success=False, output=""))
        self.assertEqual(self.snapshot(self.root), before)
        self.assertEqual(list(self.root.rglob("*.mizuki-*")), [])

    def test_overlapping_and_protected_paths_are_rejected_without_changes(self):
        before = self.snapshot(self.root)
        candidates = [
            self.content,
            self.content / "generated",
            self.content / ".." / "content" / "generated",
            self.theme,
            self.theme / "assets",
            self.root,
            self.root.parent,
            Path(self.root.anchor),
        ]
        for output in candidates:
            with self.subTest(output=output):
                self.build(success=False, output=output)
                self.assertEqual(self.snapshot(self.root), before)
                self.assertEqual(list(self.root.rglob("*.mizuki-*")), [])

    def test_output_containing_source_is_rejected(self):
        inputs = self.root / "inputs"
        inputs.mkdir()
        self.content = self.content.rename(inputs / "content")
        before = self.snapshot(self.root)
        self.assertIn("must not overlap", self.build(success=False, output=inputs))
        self.assertEqual(self.snapshot(self.root), before)
        self.assertEqual(list(self.root.rglob("*.mizuki-*")), [])

    def test_regular_file_output_is_rejected_without_changes(self):
        self.output.write_text("Keep this file.")
        before = self.snapshot(self.root)
        self.assertIn("must be a directory", self.build(success=False))
        self.assertEqual(self.snapshot(self.root), before)
        self.assert_no_temporary_directories()

    def test_symlink_output_is_rejected_including_trailing_separators(self):
        target = self.root / "existing-site"
        target.mkdir()
        (target / "manual.txt").write_text("Keep the symlink target.")
        self.make_symlink(self.output, target)
        for suffix in ("", os.sep, os.sep + "."):
            with self.subTest(suffix=suffix):
                self.assertIn(
                    "must not be a symbolic link",
                    self.build(success=False, output=str(self.output) + suffix),
                )
                self.assertTrue(self.output.is_symlink())
                self.assertEqual((target / "manual.txt").read_text(), "Keep the symlink target.")
                self.assertEqual(list(target.iterdir()), [target / "manual.txt"])
                self.assert_no_temporary_directories()

    def test_dangling_symlink_output_is_rejected(self):
        target = self.root / "missing-site"
        self.make_symlink(self.output, target)
        self.assertIn("must not be a symbolic link", self.build(success=False))
        self.assertTrue(self.output.is_symlink())
        self.assertFalse(target.exists())
        self.assert_no_temporary_directories()

    def test_source_overlap_through_parent_symlink_is_rejected(self):
        alias = self.root / "alias"
        self.make_symlink(alias, self.content)
        before = self.snapshot(self.content)
        self.assertIn(
            "must not overlap", self.build(success=False, output=alias / "generated")
        )
        self.assertEqual(self.snapshot(self.content), before)
        self.assertFalse((self.content / "generated").exists())
        self.assertEqual(list(self.root.rglob("*.mizuki-*")), [])


if __name__ == "__main__":
    unittest.main(verbosity=2)
