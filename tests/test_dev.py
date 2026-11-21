"""Black-box tests of persistent watchers, HTTP preview and SSE notifications."""

from http.client import HTTPConnection
from pathlib import Path
import re
import shutil
import socket
import subprocess
import time

from support import BuilderFixture, run_tests


class DevTests(BuilderFixture):
    def configure(self, text='title = "Initial site"\ntheme = "theme"\n', atomic=False):
        target = self.root / "mizuki.toml"
        if atomic:
            temporary = target.with_suffix(".tmp")
            temporary.write_text(text, encoding="utf-8")
            temporary.replace(target)
        else:
            target.write_text(text, encoding="utf-8")

    def logs(self):
        return self.log_path.read_text(encoding="utf-8", errors="replace")

    def start(self, command="serve", *args, configure=True):
        if configure:
            self.configure()
        with socket.socket() as probe:
            probe.bind(("127.0.0.1", 0))
            self.port = probe.getsockname()[1]
        self.log_path = self.root / "process.log"
        log = self.log_path.open("wb")
        self.addCleanup(log.close)
        command_args = [str(self.binary), command, *map(str, args)]
        if command == "serve":
            command_args.extend(["--port", str(self.port)])
        self.process = subprocess.Popen(command_args, cwd=self.root, stdout=log, stderr=subprocess.STDOUT)
        self.addCleanup(self.stop)
        self.wait_for(lambda: (self.output / "index.html").is_file(), "initial build")
        if command == "serve":
            self.wait_for(lambda: self.request("/")[0] == 200, "HTTP listener")

    def stop(self):
        if self.process.poll() is None:
            self.process.terminate()
            try:
                self.process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                self.process.kill()
                self.process.wait(timeout=5)

    def wait_for(self, predicate, description, timeout=10):
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            if self.process.poll() is not None:
                self.fail(f"Process exited during {description}:\n{self.logs()}")
            try:
                if predicate():
                    return
            except (OSError, ConnectionError):
                pass
            time.sleep(0.05)
        self.fail(f"Timed out waiting for {description}:\n{self.logs()}")

    def request(self, path, method="GET"):
        connection = HTTPConnection("127.0.0.1", self.port, timeout=2)
        try:
            connection.request(method, path)
            response = connection.getresponse()
            return response.status, dict(response.getheaders()), response.read()
        finally:
            connection.close()

    def disk_contains(self, path, text):
        path = Path(path)
        return path.is_file() and text in path.read_text(encoding="utf-8")

    def http_contains(self, path, text):
        status, _, body = self.request(path)
        return status == 200 and text.encode("utf-8") in body

    def stream(self, timeout=3):
        connection = HTTPConnection("127.0.0.1", self.port, timeout=timeout)
        connection.request("GET", "/__mizuki/events")
        response = connection.getresponse()
        self.assertEqual(response.status, 200)
        self.assertEqual(response.getheader("Content-Type"), "text/event-stream")
        self.addCleanup(connection.close)
        self.addCleanup(response.close)
        return connection, response

    def event(self, response):
        while True:
            line = response.readline()
            self.assertTrue(line, "SSE connection closed unexpectedly")
            if line.startswith(b"data: "):
                return int(line[6:])

    def version(self):
        status, _, body = self.request("/")
        self.assertEqual(status, 200)
        return int(re.search(rb'const version = "(\d+)"', body)[1])

    def test_watch_rebuilds_edits_renames_deletions_theme_and_assets(self):
        asset = self.content / "download.bin"
        asset.write_bytes(b"initial")
        self.start("watch")
        article = self.output / "posts/first.html"
        self.write_page("posts/first.md", "Changed", "Changed body.")
        self.wait_for(lambda: self.disk_contains(article, "Changed body."), "article edit")
        (self.content / "posts/first.md").rename(self.content / "posts/renamed.md")
        renamed = self.output / "posts/renamed.html"
        self.wait_for(lambda: renamed.is_file() and not article.exists(), "article rename")
        asset.write_bytes(b"changed resource")
        self.wait_for(lambda: (self.output / "download.bin").read_bytes() == b"changed resource", "static edit")
        css = self.theme / "assets/css/style.css"
        css.write_text("body { color: red; }", encoding="utf-8")
        self.wait_for(lambda: self.disk_contains(self.output / "assets/css/style.css", "red"), "theme asset edit")
        layout = self.theme / "layouts/post.html"
        layout.write_text(layout.read_text(encoding="utf-8").replace("<body>", "<body>Template edit"), encoding="utf-8")
        self.wait_for(lambda: self.disk_contains(renamed, "Template edit"), "layout edit")
        (self.content / "posts/renamed.md").unlink()
        self.wait_for(lambda: not renamed.exists(), "article deletion")
        asset.unlink()
        self.wait_for(lambda: not (self.output / "download.bin").exists(), "asset deletion")
        self.assert_no_temporary_directories()

    def test_preview_http_static_head_errors_and_sse(self):
        (self.content / "binary.bin").write_bytes(b"\x00\xff\x01\x02")
        (self.content / "中文 space.txt").write_text("UTF-8 resource", encoding="utf-8")
        (self.content / "folder").mkdir()
        (self.content / "folder/index.html").write_text("<body>Folder</body>")
        self.start()
        status, headers, body = self.request("/")
        self.assertEqual(status, 200)
        self.assertIn("text/html", headers["Content-Type"])
        self.assertEqual(headers["Cache-Control"], "no-store")
        self.assertIn(b'new EventSource("/__mizuki/events")', body)
        self.assertNotIn("EventSource", (self.output / "index.html").read_text(encoding="utf-8"))
        status, headers, body = self.request("/assets/css/style.css")
        self.assertEqual(status, 200)
        self.assertIn("text/css", headers["Content-Type"])
        self.assertEqual(body, (self.theme / "assets/css/style.css").read_bytes())
        self.assertEqual(self.request("/binary.bin")[2], b"\x00\xff\x01\x02")
        self.assertEqual(self.request("/%E4%B8%AD%E6%96%87%20space.txt")[2], b"UTF-8 resource")
        for path in ("/", "/binary.bin", "/assets/css/style.css"):
            status, headers, body = self.request(path, "HEAD")
            self.assertEqual(status, 200)
            self.assertEqual(body, b"")
            self.assertGreater(int(headers["Content-Length"]), 0)
        for path in ("/missing", "/../mizuki.toml", "/%2e%2e/mizuki.toml", "/%5c..%5cmizuki.toml", "/C:/mizuki.toml"):
            self.assertIn(self.request(path)[0], (400, 404), path)
        status, headers, _ = self.request("/folder")
        self.assertEqual(status, 301)
        self.assertEqual(headers["Location"], "/folder/")
        self.assertTrue(self.http_contains("/folder/", "Folder"))
        _, response = self.stream()
        initial = self.event(response)
        self.write_page("posts/first.md", "Changed", "Preview update.")
        self.wait_for(lambda: self.http_contains("/posts/first.html", "Preview update."), "preview update")
        self.assertGreater(self.event(response), initial)

    def test_failed_build_keeps_served_output_and_sends_no_reload_then_recovers(self):
        self.start()
        _, response = self.stream(timeout=0.6)
        initial = self.event(response)
        before = self.snapshot()
        (self.content / "posts/first.md").write_text("Missing front matter.")
        self.wait_for(lambda: "Build failed" in self.logs(), "failed build diagnostic")
        self.assertEqual(self.snapshot(), before)
        self.assertTrue(self.http_contains("/posts/first.html", "Original body."))
        self.assertEqual(self.version(), initial)
        # A failed build must not emit another data event (heartbeats are every 5 seconds).
        with self.assertRaises((TimeoutError, socket.timeout)):
            self.event(response)
        response.close()
        _, recovered = self.stream()
        self.assertEqual(self.event(recovered), initial)
        self.write_page("posts/first.md", "Recovered", "Recovered body.")
        self.wait_for(lambda: self.http_contains("/posts/first.html", "Recovered body."), "build recovery")
        self.assertGreater(self.event(recovered), initial)
        self.assert_no_temporary_directories()

    def test_config_atomic_reload_invalid_delete_recreate_and_cli_precedence(self):
        self.start("serve", "--theme", "theme", "--output", "dist")
        self.configure('title = "Reloaded title"\ntheme = "ignored-missing"\noutput = "ignored-output"\n', atomic=True)
        self.wait_for(lambda: self.http_contains("/", "Reloaded title"), "atomic configuration reload")
        initial = self.version()
        self.configure("title = [invalid")
        self.wait_for(lambda: "Failed to load" in self.logs(), "invalid configuration diagnostic")
        self.assertTrue(self.http_contains("/", "Reloaded title"))
        self.assertEqual(self.version(), initial)
        # An absent optional default config restores defaults and retains CLI paths.
        (self.root / "mizuki.toml").unlink()
        self.wait_for(lambda: self.http_contains("/", "<title>Mizuki</title>"), "configuration deletion")
        self.configure('title = "Recreated"\ntheme = "still-ignored"\n')
        self.wait_for(lambda: self.http_contains("/", "Recreated"), "configuration recreation")
        self.assertFalse((self.root / "ignored-output").exists())

    def test_config_changes_content_theme_and_output_and_new_directories_remain_watched(self):
        next_content = self.root / "next-文稿"
        next_theme = self.root / "next-主题"
        next_output = self.root / "next-站点"
        shutil.copytree(self.content, next_content)
        shutil.copytree(self.theme, next_theme)
        (next_content / "only-new.txt").write_text("New output root.")
        (next_content / "posts/first.md").write_text("Broken new source.")
        self.start()
        self.configure('title = "New site"\ncontent = "next-文稿"\ntheme = "next-主题"\noutput = "next-站点"\n')
        self.wait_for(lambda: "Broken" in self.logs() or "Failed to parse" in self.logs(), "new source failed build")
        self.assertTrue(self.http_contains("/", "Initial site"))
        self.assertEqual(self.request("/only-new.txt")[0], 404)
        self.assertFalse(next_output.exists())
        self.content = next_content
        self.theme = next_theme
        self.write_page("posts/first.md", "New article", "New content directory.")
        self.wait_for(lambda: self.http_contains("/", "New site"), "successful output switch")
        self.assertEqual(self.request("/only-new.txt")[2], b"New output root.")
        self.assertTrue(self.http_contains("/posts/first.html", "New content directory."))
        css = next_theme / "assets/css/style.css"
        css.write_text("/* Newly watched theme */", encoding="utf-8")
        self.wait_for(lambda: self.http_contains("/assets/css/style.css", "Newly watched theme"), "new theme watcher")
        self.write_page("posts/first.md", "Another update", "New watcher works.")
        self.wait_for(lambda: self.http_contains("/posts/first.html", "New watcher works."), "new content watcher")
        self.assertIn("Initial site", (self.output / "index.html").read_text(encoding="utf-8"))
        self.assert_no_temporary_directories(next_output)

    def test_initial_failed_build_serves_previous_output_and_recovers(self):
        self.build()
        before = self.snapshot()
        (self.content / "posts/first.md").write_text("Broken source.", encoding="utf-8")
        self.start()
        self.wait_for(lambda: "Build failed" in self.logs(), "initial failed build")
        self.assertEqual(self.snapshot(), before)
        self.assertTrue(self.http_contains("/posts/first.html", "Original body."))
        initial_version = self.version()
        self.write_page("posts/first.md", "Repaired", "Recovered after startup failure.")
        self.wait_for(lambda: self.http_contains("/posts/first.html", "Recovered after startup failure."),
                      "recovery after initial failure")
        self.assertGreater(self.version(), initial_version)
        self.assert_no_temporary_directories()

    def test_initial_failed_build_without_output_exits(self):
        self.configure()
        (self.content / "posts/first.md").write_text("Broken source.", encoding="utf-8")
        result = subprocess.run([str(self.binary), "serve"], cwd=self.root,
                                capture_output=True, text=True, encoding="utf-8", timeout=15)
        self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
        self.assertIn("Failed to parse", result.stdout + result.stderr)
        self.assertIn("Failed to serve directory", result.stdout + result.stderr)
        self.assertFalse(self.output.exists())
        self.assert_no_temporary_directories()

    def test_occupied_port_fails_and_cli_rejects_invalid_ports(self):
        self.configure()
        with socket.socket() as listener:
            listener.bind(("127.0.0.1", 0))
            listener.listen()
            port = listener.getsockname()[1]
            result = subprocess.run([str(self.binary), "serve", "--port", str(port)], cwd=self.root,
                                    capture_output=True, text=True, encoding="utf-8", timeout=15)
        self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
        self.assertIn("Failed to bind", result.stdout + result.stderr)
        for port in ("0", "65536", "-1", "12x"):
            result = subprocess.run([str(self.binary), "serve", "--port", port], cwd=self.root,
                                    capture_output=True, text=True, encoding="utf-8", timeout=15)
            self.assertEqual(result.returncode, 1)
            self.assertIn("Invalid port", result.stdout + result.stderr)


if __name__ == "__main__":
    run_tests()
