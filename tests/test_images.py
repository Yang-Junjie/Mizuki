"""Regressions for opt-in image optimization and its persistent cache."""

import struct
import zlib

from support import BuilderFixture, run_tests


def chunk(name, data):
    body = name + data
    return struct.pack(">I", len(data)) + body + struct.pack(">I", zlib.crc32(body))


PNG_SIGNATURE = b"\x89PNG\r\n\x1a\n"
PNG_HEADER = chunk(b"IHDR", struct.pack(">IIBBBBB", 64, 64, 8, 6, 0, 0, 0))
PNG_METADATA = chunk(b"tEXt", b"Comment\x00Preserve this")
PNG_END = chunk(b"IEND", b"")


def png(scanlines):
    packed = zlib.compress(scanlines, level=0)
    return (PNG_SIGNATURE + PNG_HEADER + PNG_METADATA
            + chunk(b"IDAT", packed[:50]) + chunk(b"IDAT", packed[50:]) + PNG_END)


class ImageTests(BuilderFixture):
    def test_png_is_opt_in_lossless_and_cached(self):
        scanlines = (b"\0" + b"\xff\x9a\x63\x7f" * 64) * 64
        original = png(scanlines)
        (self.content / "diagram.PNG").write_bytes(original)
        (self.theme / "assets/duplicate.png").write_bytes(original)
        (self.content / "broken.jpg").write_bytes(b"copy invalid assets unchanged")
        self.build()
        result = self.output / "diagram.PNG"
        self.assertEqual(result.read_bytes(), original)

        (self.root / "mizuki.toml").write_text(
            "[build.images]\ncompress = true\n", encoding="utf-8"
        )
        log = self.build()
        self.assertIn("Images: 2 optimized, 1 cache hits", log)
        optimized = result.read_bytes()
        self.assertLess(len(optimized), len(original))
        prefix = PNG_SIGNATURE + PNG_HEADER + PNG_METADATA
        self.assertTrue(optimized.startswith(prefix))
        self.assertTrue(optimized.endswith(PNG_END))
        self.assertEqual(zlib.decompress(optimized[len(prefix) + 8 : -len(PNG_END) - 4]), scanlines)
        self.assertEqual((self.content / "diagram.PNG").read_bytes(), original)
        self.assertEqual((self.output / "assets/duplicate.png").read_bytes(), optimized)
        self.assertEqual((self.output / "broken.jpg").read_bytes(), b"copy invalid assets unchanged")

        self.assertIn("Images: 2 optimized, 3 cache hits", self.build())
        cache = self.root / ".cache/mizuki/images"
        entry = next(cache.glob("v1-png-*"))
        entry.write_bytes(b"corrupt")
        self.assertIn("Images: 2 optimized, 2 cache hits", self.build())
        self.assertEqual(result.read_bytes(), optimized)
        self.assertIn("Images: 2 optimized, 3 cache hits", self.build())

    def test_image_options_validate_before_publishing(self):
        self.build()
        before = self.snapshot()
        for text, diagnostic in [
            ("build = true", "'build' must be a table"),
            ("[build]\nimages = true", "'build.images' must be a table"),
            ('[build.images]\ncompress = "true"', "'build.images.compress' must be a boolean"),
            ("[build.images]\nquality = 101", "'build.images.quality' must be an integer from 1 to 100"),
            ("[build.images]\nquality = 85.5", "'build.images.quality' must be an integer from 1 to 100"),
        ]:
            with self.subTest(text=text):
                (self.root / "mizuki.toml").write_text(text, encoding="utf-8")
                self.assertIn(diagnostic, self.build(success=False))
                self.assertEqual(self.snapshot(), before)


if __name__ == "__main__":
    run_tests()
