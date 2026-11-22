"""Global CLI options must work without loading a site or writing files."""

from pathlib import Path
import os
import subprocess
import tempfile
import unittest

from support import binary_path, run_tests


class CliTests(unittest.TestCase):
    def setUp(self):
        self.binary = binary_path()
        temporary = tempfile.TemporaryDirectory(prefix="mizuki-cli-")
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        (self.root / "mizuki.toml").write_text("invalid = [", encoding="utf-8")

    def command(self, *args):
        return subprocess.run([str(self.binary), *args], cwd=self.root,
                              capture_output=True, text=True, encoding="utf-8", timeout=10)

    def test_version_ignores_invalid_site_configuration(self):
        for option in ["--version", "-V"]:
            with self.subTest(option=option):
                result = self.command(option)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                self.assertRegex(result.stdout, r"\Amizuki [0-9]+\.[0-9]+\.[0-9]+\n\Z")
                expected = os.environ.get("MIZUKI_EXPECTED_VERSION")
                if expected:
                    self.assertEqual(result.stdout, f"mizuki {expected}\n")
                self.assertEqual(result.stderr, "")
                self.assertEqual(list(self.root.iterdir()), [self.root / "mizuki.toml"])

    def test_version_rejects_extra_arguments(self):
        for args in [("--version", "build"), ("-V", "--config", "other.toml")]:
            with self.subTest(args=args):
                result = self.command(*args)
                self.assertEqual(result.returncode, 1)
                self.assertIn("does not accept arguments", result.stdout + result.stderr)
                self.assertEqual(list(self.root.iterdir()), [self.root / "mizuki.toml"])

    def test_version_is_a_global_option(self):
        result = self.command("init", "blog", "--version")
        self.assertEqual(result.returncode, 1)
        self.assertFalse((self.root / "blog").exists())

    def test_help_includes_version_option(self):
        result = self.command("--help")
        self.assertEqual(result.returncode, 0)
        self.assertIn("--version, -V", result.stdout)


if __name__ == "__main__":
    run_tests()
