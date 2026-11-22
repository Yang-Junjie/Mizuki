/* Optional browser regression: NODE_PATH=/path/to/node_modules node tests/browser_reading.cjs build/bin/mizuki
 * Requires playwright-core and Chromium (MIZUKI_CHROME can override its executable).
 */
const assert = require("node:assert/strict");
const fs = require("node:fs");
const http = require("node:http");
const os = require("node:os");
const path = require("node:path");
const { execFileSync } = require("node:child_process");
const { chromium } = require("playwright-core");

const project = path.resolve(__dirname, "..");
const binary = path.resolve(process.argv[2] || "build/bin/mizuki");
const root = fs.mkdtempSync(path.join(os.tmpdir(), "mizuki-reading-browser-"));
const site = path.join(root, "site");
const docs = path.join(root, "docs");
function run(args, cwd = root) {
  return execFileSync(binary, args, { cwd, encoding: "utf8" });
}
const code = '#include <iostream>\nint main() { std::cout << "<tag> & $literal$"; return 0; }\n';
const long = "x".repeat(300);
const body = `+++
title = "Browser reading"
+++
## Formula $x^2$

Inline $e^{i\\pi}+1=0$ and escaped \\$5.

$$
\\sum_{n=1}^{\\infty} \\frac{1}{n^2} = \\frac{\\pi^2}{6}
$$

$$
${"x+".repeat(180)}y
$$

Invalid $\\unknownCommand{x}$.

A note[^n] and again[^n].

[^n]: A **static** footnote.

> [!WARNING]
> Keep a backup.

\`\`\`cpp
${code}\`\`\`

\`\`\`unsupported_language
${long}
\`\`\`

\`\`\`
plain $notMath$
\`\`\`
`;

async function main() {
  run(["init", site]);
  fs.writeFileSync(path.join(site, "content/pages/reading.md"), body);
  run(["build", "--base-url", "/blog/"], site);
  // Exercise identical adversarial content through both actual themes.
  const fixture = path.join(root, "docs-content");
  fs.cpSync(path.join(project, "docs/content"), fixture, { recursive: true });
  fs.writeFileSync(path.join(fixture, "pages/reading.md"), body);
  run(["build", "--config", path.join(project, "docs/mizuki.toml"), "--content", fixture,
    "--output", docs, "--base-url", "/docs/"]);

  const mime = { ".html": "text/html", ".css": "text/css", ".js": "text/javascript", ".woff2": "font/woff2" };
  const server = http.createServer((request, response) => {
    const url = decodeURIComponent(new URL(request.url, "http://localhost").pathname);
    const isDocs = url.startsWith("/docs/");
    const prefix = isDocs ? "/docs/" : "/blog/";
    const file = path.join(isDocs ? docs : path.join(site, "dist"), url.slice(prefix.length) || "index.html");
    try {
      const data = fs.readFileSync(file);
      response.writeHead(200, { "Content-Type": mime[path.extname(file)] || "application/octet-stream" });
      response.end(data);
    } catch {
      response.writeHead(404);
      response.end();
    }
  });
  await new Promise((resolve) => server.listen(0, "127.0.0.1", resolve));
  const origin = `http://127.0.0.1:${server.address().port}`;
  let browser;
  try {
    browser = await chromium.launch({ executablePath: process.env.MIZUKI_CHROME || "/usr/bin/google-chrome-stable", headless: true });
    for (const theme of ["blog", "docs"]) {
      for (const colorScheme of ["light", "dark"]) {
        for (const width of [390, 1440]) {
          const context = await browser.newContext({ colorScheme, viewport: { width, height: 950 },
            permissions: ["clipboard-read", "clipboard-write"] });
          const errors = [], requests = [], external = [], warnings = [];
          await context.route("**/*", (route) => {
            if (route.request().url().startsWith(origin + "/")) return route.continue();
            external.push(route.request().url());
            return route.abort();
          });
          const page = await context.newPage();
          page.on("pageerror", (error) => errors.push(error.message));
          page.on("response", (response) => { if (response.status() >= 400) errors.push(response.url()); });
          page.on("request", (request) => requests.push(request.url()));
          page.on("console", (message) => { if (message.type() === "warning") warnings.push(message.text()); });
          await page.goto(`${origin}/${theme}/pages/reading.html`, { waitUntil: "networkidle" });
          await page.evaluate(() => document.fonts.ready);
          assert.equal(await page.locator("code.language-cpp .hljs-keyword").count() > 0, true);
          assert.equal(await page.locator("code.language-unsupported_language span").count(), 0);
          assert.equal(await page.locator("[data-markdown] pre > code:not([class])").textContent(), "plain $notMath$\n");
          assert.equal(await page.locator("x-equation.math-rendered math").count(), 4);
          assert.equal(await page.locator("x-equation.math-error").textContent(), "\\unknownCommand{x}");
          assert.equal(warnings.length, 1);
          assert.equal(warnings[0].startsWith("Math rendering failed:"), true);
          assert.equal(requests.some((url) => url.endsWith(".woff2")), true);
          const sizing = await page.evaluate(() => ({
            width: document.documentElement.clientWidth,
            scroll: document.documentElement.scrollWidth,
            code: [...document.querySelectorAll("[data-markdown] pre")].some((node) => node.scrollWidth > node.clientWidth),
            math: [...document.querySelectorAll('x-equation[type="display"]')].some((node) => node.scrollWidth > node.clientWidth),
            keyword: getComputedStyle(document.querySelector(".hljs-keyword")).color,
          }));
          assert.ok(sizing.scroll <= sizing.width + 1, JSON.stringify({ theme, width, colorScheme, sizing }));
          assert.ok(sizing.code && sizing.math);
          assert.equal(sizing.keyword, colorScheme === "dark" ? "rgb(255, 143, 163)" : "rgb(163, 21, 66)");
          await page.addStyleTag({ content: ":root { --syntax-keyword: #123456; --code-font-size: 17px; --math-font-size: 22px; --footnote-font-size: 13px; --admonition-warning: #654321; }" });
          const custom = await page.evaluate(() => ({
            keyword: getComputedStyle(document.querySelector(".hljs-keyword")).color,
            code: getComputedStyle(document.querySelector("[data-markdown] pre")).fontSize,
            math: getComputedStyle(document.querySelector(".katex")).fontSize,
            footnote: getComputedStyle(document.querySelector(".footnotes")).fontSize,
            warning: getComputedStyle(document.querySelector(".admonition-title")).color,
          }));
          assert.deepEqual(custom, { keyword: "rgb(18, 52, 86)", code: "17px", math: "22px", footnote: "13px", warning: "rgb(101, 67, 33)" });
          const button = page.locator(".code-block").filter({ has: page.locator(".language-cpp") }).locator("button");
          await button.click();
          assert.equal(await page.evaluate(() => navigator.clipboard.readText()), code);
          assert.equal(await button.textContent(), "已复制");
          await page.evaluate(() => { navigator.clipboard.writeText = async () => { throw new Error("Clipboard denied"); }; });
          await button.click();
          assert.equal(await button.textContent(), "复制失败");
          await page.locator('a[href="#fn-1"]').first().click();
          assert.equal(new URL(page.url()).hash, "#fn-1");
          await page.locator(".footnote-backref").last().click();
          assert.equal(new URL(page.url()).hash, "#fnref-1-2");
          assert.deepEqual(errors, []);
          assert.deepEqual(external, []);
          await context.close();
        }
      }
      const context = await browser.newContext({ javaScriptEnabled: false, viewport: { width: 390, height: 950 } });
      const page = await context.newPage();
      await page.goto(`${origin}/${theme}/pages/reading.html`);
      assert.equal(await page.locator("code.language-cpp").textContent(), code);
      assert.equal(await page.locator(".code-copy, .katex").count(), 0);
      assert.equal(await page.locator("x-equation").first().textContent(), "x^2");
      await page.locator('a[href="#fn-1"]').first().click();
      assert.equal(new URL(page.url()).hash, "#fn-1");
      await page.locator(".footnote-backref").last().click();
      assert.equal(new URL(page.url()).hash, "#fnref-1-2");
      await context.close();
    }
    console.log("Passed: both themes, light/dark, mobile/desktop, offline assets/fonts, math/error fallback, clipboard and static footnotes.");
  } finally {
    if (browser) await browser.close();
    await new Promise((resolve) => server.close(resolve));
  }
}
main().catch((error) => { console.error(error); process.exitCode = 1; }).finally(() => fs.rmSync(root, { recursive: true, force: true }));
