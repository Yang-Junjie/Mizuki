"""Site creation, content creation and preservation of existing user files."""

from datetime import datetime
from html import unescape
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

from support import DEFAULT_THEME, binary_path, run_tests


class ScaffoldTests(unittest.TestCase):
    def setUp(self):
        self.binary = binary_path()
        temporary = tempfile.TemporaryDirectory(prefix="mizuki-scaffold-")
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)

    def command(self, *args, success=True, cwd=None, binary=None):
        result = subprocess.run(
            [str(binary or self.binary), *map(str, args)],
            cwd=cwd or self.root, capture_output=True, text=True, encoding="utf-8", timeout=15,
        )
        message = result.stdout + result.stderr
        self.assertEqual(result.returncode, 0 if success else 1, message)
        return message

    def init(self):
        site = self.root / "blog"
        self.command("init", site)
        return site

    def snapshot(self):
        return {
            path.relative_to(self.root).as_posix(): path.read_bytes() if path.is_file() else None
            for path in self.root.rglob("*")
        }

    def symlink(self, link, target, directory=False):
        try:
            link.symlink_to(target, target_is_directory=directory)
        except (OSError, NotImplementedError) as error:
            self.skipTest(f"Symlinks unavailable: {error}")

    def test_copied_binary_initializes_an_independent_buildable_site(self):
        tools = self.root / "tools"
        tools.mkdir()
        binary = tools / self.binary.name
        shutil.copy2(self.binary, binary)
        for library in self.binary.parent.glob("*.dll"):
            shutil.copy2(library, tools / library.name)
        site = self.root / "新站点 with spaces"
        self.command("init", site, binary=binary)
        expected = {
            p.relative_to(DEFAULT_THEME).as_posix(): p.read_bytes()
            for p in DEFAULT_THEME.rglob("*") if p.is_file()
        }
        actual = {
            p.relative_to(site / "themes/default").as_posix(): p.read_bytes()
            for p in (site / "themes/default").rglob("*") if p.is_file()
        }
        self.assertEqual(actual, expected)
        self.assertEqual((site / ".gitignore").read_text(), "/dist/\n/.cache/\n")
        self.command("build", cwd=site, binary=binary)
        for name in ("index.html", "archives/index.html", "posts/welcome.html", "pages/about.html",
                     "assets/css/style.css"):
            self.assertTrue((site / "dist" / name).is_file(), name)
        home = (site / "dist/index.html").read_text(encoding="utf-8")
        self.assertIn("欢迎使用 Mizuki", home)
        self.assertIn('href="archives/index.html"', home)
        self.assertIn('href="pages/about.html"', home)
        welcome = (site / "dist/posts/welcome.html").read_text(encoding="utf-8")
        self.assertIn('class="toc"', welcome)
        self.assertIn('id="开始写作"', welcome)
        self.assertIn('id="预览站点"', welcome)
        self.assertIn('href="#%E5%BC%80%E5%A7%8B%E5%86%99%E4%BD%9C"', welcome)

    def test_init_accepts_existing_empty_directory_and_current_directory(self):
        for target in ("empty", "."):
            with self.subTest(target=target):
                working = self.root / ("existing" if target == "empty" else "current")
                working.mkdir()
                if target == "empty":
                    (working / target).mkdir()
                self.command("init", target, cwd=working)
                site = working / target
                self.command("build", cwd=site)
                self.assertTrue((site / "dist/index.html").is_file())

    def test_init_creates_nested_directories(self):
        self.command("init", "sites/中文/blog/")
        self.command("build", cwd=self.root / "sites/中文/blog")

    def test_init_refuses_nonempty_directory_including_hidden_files(self):
        site = self.root / "blog"
        site.mkdir()
        (site / ".keep").write_bytes(b"User content\x00")
        before = self.snapshot()
        self.assertIn("must be empty", self.command("init", site, success=False))
        self.assertEqual(self.snapshot(), before)

    def test_init_refuses_file_and_reinitialization_without_changes(self):
        existing = self.root / "existing"
        existing.write_bytes(b"User file")
        before = self.snapshot()
        self.command("init", existing, success=False)
        self.assertEqual(self.snapshot(), before)
        site = self.init()
        before = self.snapshot()
        self.command("init", site, success=False)
        self.assertEqual(self.snapshot(), before)

    def test_init_refuses_final_symlink_even_with_trailing_dot(self):
        target = self.root / "target"
        target.mkdir()
        link = self.root / "alias"
        self.symlink(link, target, directory=True)
        for suffix in ("", "/", "/."):
            with self.subTest(suffix=suffix):
                self.command("init", str(link) + suffix, success=False)
                self.assertTrue(link.is_symlink())
                self.assertEqual(list(target.iterdir()), [])
        dangling = self.root / "dangling"
        self.symlink(dangling, self.root / "absent", directory=True)
        self.command("init", dangling, success=False)
        self.assertTrue(dangling.is_symlink())
        self.assertFalse((self.root / "absent").exists())

    def test_creation_help_and_argument_errors_do_not_modify_files(self):
        (self.root / "mizuki.toml").write_text("Not valid TOML!", encoding="utf-8")
        before = self.snapshot()
        for args in (("--help",), ("init", "--help"), ("new", "--help")):
            with self.subTest(args=args):
                self.assertIn("Usage:", self.command(*args))
                self.assertEqual(self.snapshot(), before)
        for args in (
            ("init",), ("init", "one", "two"), ("init", "blog", "--config", "site.toml"),
            ("init", ""), ("new",), ("new", "posts/file.md"), ("new", "--title", "Title"),
            ("new", "posts/file.md", "--title"),
            ("new", "posts/file.md", "--title", "One", "--title", "Two"),
            ("new", "posts/file.md", "--title", "Title", "--description", "One", "--description", "Two"),
            ("new", "posts/file.md", "--title", "Title", "extra.md"),
        ):
            with self.subTest(args=args):
                self.command(*args, success=False)
                self.assertEqual(self.snapshot(), before)

    def test_creation_commands_reject_options_from_other_commands(self):
        before = self.snapshot()
        for option in ("--theme", "--output", "--base-url", "--port"):
            with self.subTest(option=option):
                self.command("new", "post.md", "--title", "Title", option, "value", success=False)
                self.assertEqual(self.snapshot(), before)
        self.command("build", "--draft", success=False)
        self.assertEqual(self.snapshot(), before)

    def test_new_writes_local_date_and_publishes_by_default(self):
        site = self.init()
        before = datetime.now().date().isoformat()
        self.command("new", "posts/第一篇.md", "--title", "第一篇文章", cwd=site)
        after = datetime.now().date().isoformat()
        source = (site / "content/posts/第一篇.md").read_text(encoding="utf-8")
        self.assertTrue(source.startswith("+++\n"))
        date_line = next(line for line in source.splitlines() if line.startswith("date = "))
        self.assertIn(date_line[8:-1], (before, after))
        self.assertIn("draft = false\n", source)
        self.assertIn('description = ""\n', source)
        self.command("build", cwd=site)
        self.assertIn("第一篇文章", (site / "dist/index.html").read_text(encoding="utf-8"))
        self.assertTrue((site / "dist/posts/第一篇.html").is_file())

    def test_new_round_trips_quotes_backslashes_and_spaces(self):
        site = self.init()
        titles = ('中文: # "double" and \'single\' \\ end', '"quoted"', "'quoted'", "  spaced title  ", "title\twith tab", "+++")
        description = '摘要: "引号" \\ <tag> & \'single\''
        for index, title in enumerate(titles):
            self.command("new", f"posts/quotes-{index}.md", "--title", title,
                         "--description", description, cwd=site)
        self.command("build", cwd=site)
        for index, title in enumerate(titles):
            page = (site / f"dist/posts/quotes-{index}.html").read_text(encoding="utf-8")
            self.assertIn(f"<h1>{title}</h1>", unescape(page))
            self.assertIn(f'content="{description}"', unescape(page))

    def test_new_draft_is_excluded_from_build_and_archive(self):
        site = self.init()
        self.command("new", "posts/hidden.md", "--title", "Hidden draft", "--draft", cwd=site)
        self.assertIn("draft = true\n", (site / "content/posts/hidden.md").read_text())
        self.command("build", cwd=site)
        self.assertFalse((site / "dist/posts/hidden.html").exists())
        for name in ("index.html", "archives/index.html"):
            self.assertNotIn("Hidden draft", (site / "dist" / name).read_text(encoding="utf-8"))

    def test_new_independent_page_is_not_listed_as_an_article(self):
        site = self.init()
        self.command("new", "pages/extra.md", "--title", "Extra independent page", cwd=site)
        self.command("build", cwd=site)
        self.assertIn("Extra independent page", (site / "dist/pages/extra.html").read_text(encoding="utf-8"))
        self.assertNotIn("Extra independent page", (site / "dist/index.html").read_text(encoding="utf-8"))

    def test_new_uses_config_relative_content_and_cli_override_relative_to_cwd(self):
        config = self.root / "settings/站点.toml"
        config.parent.mkdir()
        config.write_text('content = "../文稿"\n', encoding="utf-8")
        self.command("new", "notes/one.md", "--title", "One", "--config", config)
        self.assertTrue((self.root / "文稿/notes/one.md").is_file())
        for index, args in enumerate((("--content", "override", "--config", config),
                                      ("--config", config, "--content", "override"))):
            self.command("new", f"notes/{index}.md", "--title", "Override", *args)
            self.assertTrue((self.root / f"override/notes/{index}.md").is_file())
        self.assertFalse((self.root / "content").exists())

    def test_new_without_config_uses_defaults_and_creates_parent_directories(self):
        self.command("new", "notes/deep/post.MD", "--title", "Default directory")
        self.assertTrue((self.root / "content/notes/deep/post.MD").is_file())
        self.assertFalse((self.root / "mizuki.toml").exists())

    def test_new_refuses_existing_file_and_directory(self):
        site = self.init()
        directory = site / "content/posts/folder.md"
        directory.mkdir()
        for name in ("posts/welcome.md", "posts/folder.md"):
            with self.subTest(name=name):
                before = self.snapshot()
                self.command("new", name, "--title", "Replacement", cwd=site, success=False)
                self.assertEqual(self.snapshot(), before)

    def test_new_rejects_invalid_paths_before_creating_directories(self):
        before = self.snapshot()
        for name in ("", "../escape.md", "posts/../../escape.md", "posts/../other.md",
                     str(self.root / "absolute.md"), "index.md", "./index.MD", "post.html", "posts/"):
            with self.subTest(name=name):
                self.command("new", name, "--title", "Title", success=False)
                self.assertEqual(self.snapshot(), before)

    def test_new_rejects_invalid_metadata_before_creating_directories(self):
        before = self.snapshot()
        for option, text in (("--title", ""), ("--title", " \t "), ("--title", "one\ntwo"),
                             ("--title", "one\rtwo"), ("--description", "one\ntwo"),
                             ("--description", "one\x01two")):
            args = (option, text) if option == "--title" else ("--title", "Title", option, text)
            with self.subTest(option=option, text=text):
                self.command("new", "nested/post.md", *args, success=False)
                self.assertEqual(self.snapshot(), before)

    def test_new_does_not_write_when_configuration_is_invalid_or_missing(self):
        before = self.snapshot()
        self.command("new", "post.md", "--title", "Title", "--config", "missing.toml", success=False)
        self.assertEqual(self.snapshot(), before)
        (self.root / "mizuki.toml").write_text("Bad TOML!")
        before = self.snapshot()
        self.command("new", "post.md", "--title", "Title", success=False)
        self.assertEqual(self.snapshot(), before)

    def test_new_refuses_symlink_parents_and_existing_dangling_symlinks(self):
        site = self.init()
        outside = self.root / "outside"
        outside.mkdir()
        parent = site / "content/alias"
        self.symlink(parent, outside, directory=True)
        self.command("new", "alias/post.md", "--title", "Title", cwd=site, success=False)
        self.assertEqual(list(outside.iterdir()), [])
        link = site / "content/posts/link.md"
        destination = outside / "missing.md"
        self.symlink(link, destination)
        self.command("new", "posts/link.md", "--title", "Title", cwd=site, success=False)
        self.assertTrue(link.is_symlink())
        self.assertFalse(destination.exists())

    def test_double_dash_allows_paths_starting_with_dash(self):
        self.command("init", "--", "-blog")
        site = self.root / "-blog"
        self.command("new", "--title", "Dash path", "--", "-article.md", cwd=site)
        self.assertTrue((site / "content/-article.md").is_file())
        self.command("build", cwd=site)


if __name__ == "__main__":
    run_tests()
