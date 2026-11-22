#!/usr/bin/env python3
"""Refresh the pinned, offline browser dependencies in both bundled themes."""

from pathlib import Path
import base64
import hashlib
import io
import re
import tarfile
import urllib.request

ROOT = Path(__file__).resolve().parents[1]
THEMES = (ROOT / "themes/default", ROOT / "docs/theme")
PACKAGES = {
    "highlight": (
        "11.12.0",
        "https://registry.npmjs.org/@highlightjs/cdn-assets/-/cdn-assets-11.12.0.tgz",
        "KvOKXODaiFmId9xaq3xc5xCL66wVLUuOngDbO9B/kewbFTqdGbn2nJxNhN3H5R1cgDTVj6R8vH0zgiNDEGjpDw==",
    ),
    "katex": (
        "0.19.1",
        "https://registry.npmjs.org/katex/-/katex-0.19.1.tgz",
        "jG8uKntjSEcm2EpQOh0XturpPuuC9r3Sq5HLHdsFdWZT27E672SF19QEi+of0/YeOy15lDgRhKvr5614uRrEfA==",
    ),
}


def download(url, integrity):
    with urllib.request.urlopen(url, timeout=60) as response:
        data = response.read()
    actual = base64.b64encode(hashlib.sha512(data).digest()).decode()
    if actual != integrity:
        raise RuntimeError(f"SHA-512 mismatch for {url}")
    return tarfile.open(fileobj=io.BytesIO(data), mode="r:gz")


def read(archive, name):
    with archive.extractfile("package/" + name) as source:
        return source.read()


def main():
    assets = {}
    version, url, integrity = PACKAGES["highlight"]
    with download(url, integrity) as archive:
        # The official common bundle already includes ini/TOML and the usual web languages.
        bundle = read(archive, "highlight.min.js") + b"\n" + read(archive, "languages/cmake.min.js")
        assets["highlight/highlight.min.js"] = bundle
        assets["highlight/LICENSE"] = read(archive, "LICENSE")
        assets["highlight/VERSION"] = (version + "\n").encode()
    version, url, integrity = PACKAGES["katex"]
    with download(url, integrity) as archive:
        css = read(archive, "dist/katex.min.css").decode()
        # Ship WOFF2 only. Keep all other upstream CSS and every referenced font intact.
        css = re.sub(r'url\(fonts/[^)]+\.woff\) format\("woff"\),?', '', css)
        css = re.sub(r'url\(fonts/[^)]+\.ttf\) format\("truetype"\)', '', css)
        css = css.replace('format("woff2"),', 'format("woff2")')
        assets["katex/katex.min.css"] = css.encode()
        assets["katex/katex.min.js"] = read(archive, "dist/katex.min.js")
        assets["katex/LICENSE"] = read(archive, "LICENSE")
        assets["katex/VERSION"] = (version + "\n").encode()
        for font in sorted(set(re.findall(r'url\((fonts/[^)]+)\)', css))):
            assets["katex/" + font] = read(archive, "dist/" + font)
    font_license_url = "https://raw.githubusercontent.com/KaTeX/katex-fonts/56e79c93c88c9054b017cc92e496b383e0bf82d7/LICENSE"
    with urllib.request.urlopen(font_license_url, timeout=60) as response:
        font_license = response.read()
    if hashlib.sha256(font_license).hexdigest() != "a7bd44abd8419bf78185be51f34216356af1adb0806ce39d6a9ca0940bd49b70":
        raise RuntimeError("KaTeX fonts license SHA-256 mismatch")
    assets["katex/fonts/LICENSE"] = font_license
    assets["README.txt"] = (
        "Offline reading assets, maintained by tools/update_reading_assets.py.\n"
        "highlight.js: official common browser bundle plus CMake, BSD-3-Clause.\n"
        "KaTeX: official browser bundle and CSS; font sources reduced to WOFF2, MIT.\n"
        "See each package's LICENSE and VERSION. No CDN is needed at runtime.\n"
    ).encode()
    for theme in THEMES:
        for name, content in assets.items():
            target = theme / "assets/vendor" / name
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(content)
    print(f"Updated {len(assets)} files in each theme")


if __name__ == "__main__":
    main()
