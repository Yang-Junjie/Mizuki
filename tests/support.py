"""Shared CLI fixtures and an explicit script entry point for the regression suites."""

from pathlib import Path
import os
import shutil
import subprocess
import sys
import tempfile
import unittest


PROJECT_ROOT = Path(__file__).resolve().parents[1]
DEFAULT_THEME = PROJECT_ROOT / "themes" / "default"


def binary_path():
    default = PROJECT_ROOT / "build" / "bin" / ("mizuki.exe" if os.name == "nt" else "mizuki")
    binary = Path(os.environ.get("MIZUKI_BINARY", default)).resolve()
    if not binary.is_file():
        raise RuntimeError(f"Build Mizuki first or set MIZUKI_BINARY; executable missing: {binary}")
    return binary


def run_tests():
    args = list(sys.argv)
    if len(args) > 1 and not args[1].startswith("-"):
        os.environ["MIZUKI_BINARY"] = str(Path(args.pop(1)).resolve())
    unittest.main(argv=args, verbosity=2)


class BuilderFixture(unittest.TestCase):
    def setUp(self):
        self.binary = binary_path()
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

    def write_page(self, name, title, body, draft=False, date="2026-11-16", description=None, atomic=False):
        path = self.content / name
        path.parent.mkdir(parents=True, exist_ok=True)
        summary = "" if description is None else f"description: {description}\n"
        text = (f"---\ntitle: {title}\ndate: {date}\n"
                f"draft: {'true' if draft else 'false'}\n{summary}---\n\n{body}\n")
        if atomic:
            # Stage outside watched inputs so no valid intermediate source is published.
            with tempfile.TemporaryDirectory(dir=self.root) as staging:
                source = Path(staging) / path.name
                source.write_text(text, encoding="utf-8")
                source.replace(path)
        else:
            path.write_text(text, encoding="utf-8")

    def build(self, *, success=True, output=None, cwd=None):
        result = subprocess.run(
            [
                str(self.binary), "build",
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
