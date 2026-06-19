import { chromium } from 'playwright';
import fs from 'fs';

const CHROME_BIN = '/home/molandtoxx/PureCloak/chromium_src/out/purecloak/chrome';
const USER_DATA_DIR = '/tmp/pc-test-webui-' + Date.now();
const BASE_URL = 'purecloak://';

// Write Local State pref to enable internal debug pages (for discards)
fs.mkdirSync(USER_DATA_DIR, { recursive: true });
const localState = {
  "internal_only_uis_enabled": true,
};
fs.writeFileSync(USER_DATA_DIR + '/Local State', JSON.stringify(localState));

const PAGES = [
  { name: 'settings',       url: 'settings',       expect: /settings|设置|Preferences/i },
  { name: 'history',        url: 'history',         expect: /history|历史/i },
  { name: 'bookmarks',      url: 'bookmarks',       expect: /bookmarks|书签/i },
  { name: 'extensions',     url: 'extensions',      expect: /extensions|扩展程序/i },
  { name: 'version',        url: 'version',         expect: /Version|版本|\d+\.\d+/i },
  { name: 'flags',          url: 'flags',           expect: /Experiments|实验|Flags/i },
  { name: 'downloads',      url: 'downloads',       expect: /downloads|下载/i },
  { name: 'help',           url: 'help',            expect: /help|关于|About/i },
  { name: 'net-internals',  url: 'net-internals',   expect: /net.?internals/i },
  { name: 'device-log',     url: 'device-log',      expect: /Device Log|设备日志|Log|日志/i },
  { name: 'blob-internals', url: 'blob-internals',  expect: /Blob|blob/i },
  { name: 'discards',       url: 'discards',        expect: /Discards|Tab|丢弃/i },
  { name: 'dino',           url: 'dino',            expect: /dino|运行|开始|空格|游戏/i },
  { name: 'new-tab',        url: 'newtab',          expect: /./ }, // just check it loads
];

async function main() {
  const results = [];

  const browser = await chromium.launchPersistentContext(USER_DATA_DIR, {
    executablePath: CHROME_BIN,
    args: [
      '--no-first-run',
      '--disable-fre',
      '--no-default-browser-check',
      '--disable-search-engine-choice-screen',
    ],
    headless: true,
    ignoreHTTPSErrors: true,
  });

  let page = await browser.newPage();

  for (const p of PAGES) {
    const url = BASE_URL + p.url;
    console.log(`\n=== Testing ${p.name} (${url}) ===`);
    try {
      const start = Date.now();
      let resp;
      let navError = null;
      if (p.name === 'dino') {
        // purecloak://dino is the offline error page — it always returns
        // ERR_INTERNET_DISCONNECTED. Navigate offline and catch the error.
        await browser.setOffline(true);
        try {
          resp = await page.goto(url, { waitUntil: 'domcontentloaded', timeout: 30000 });
        } catch (e) {
          navError = e.message;
        }
      } else {
        resp = await page.goto(url, { waitUntil: 'domcontentloaded', timeout: 30000 });
      }
      const loadTime = Date.now() - start;
      // Wait a bit for async rendering
      await page.waitForTimeout(2000);

      const title = await page.title();
      const bodyText = await page.evaluate(() => document.body?.innerText?.substring(0, 500) || '');
      const errorDisplayed = await page.evaluate(() => {
        // Check for error pages
        const err = document.querySelector('#main-message')?.innerText || '';
        const errorCode = document.querySelector('#error-code')?.innerText || '';
        return { err, errorCode };
      });

      const statusCode = resp?.status() || 0;

      // For dino page, ERR_INTERNET_DISCONNECTED is expected — check content instead
      const passed = p.name === 'dino'
        ? p.expect.test(bodyText + title)
        : p.expect.test(bodyText + title);

      console.log(`  Status: ${statusCode}, Load: ${loadTime}ms`);
      console.log(`  Title: "${title}"`);
      if (navError) console.log(`  Nav error (expected for dino): ${navError.substring(0, 100)}`);
      console.log(`  Error? ${errorDisplayed.err || 'none'}`);
      if (errorDisplayed.errorCode) console.log(`  Error code: ${errorDisplayed.errorCode}`);

      if (!passed && (errorDisplayed.err || statusCode >= 400)) {
        console.log(`  ❌ FAIL - expected "${p.expect}" but got error`);
        results.push({ name: p.name, url, passed: false, error: errorDisplayed.err || `status=${statusCode}`, loadTime });
      } else if (!passed) {
        console.log(`  ⚠️  WARN - page loaded but content doesn't match expected pattern`);
        results.push({ name: p.name, url, passed: false, error: 'content_mismatch', loadTime, body: bodyText.substring(0, 200) });
      } else {
        console.log(`  ✅ PASS`);
        results.push({ name: p.name, url, passed: true, loadTime });
      }

      // Take screenshot
      await page.screenshot({ path: `/tmp/pc-test-screenshots/${p.name}.png`, fullPage: false }).catch(() => {});
    } catch (e) {
      console.log(`  ❌ ERROR: ${e.message.substring(0, 200)}`);
      results.push({ name: p.name, url, passed: false, error: e.message.substring(0, 200) });
    } finally {
      // Restore online mode after dino test
      if (p.name === 'dino') {
        await browser.setOffline(false);
      }
      // Navigate to a clean page before new-tab to clear error state from dino
      if (p.name === 'dino') {
        await page.goto(BASE_URL + 'settings', { waitUntil: 'domcontentloaded', timeout: 10000 }).catch(() => {});
      }
    }
  }

  await browser.close();

  console.log('\n\n=============== SUMMARY ===============');
  let passed = 0, failed = 0;
  for (const r of results) {
    const icon = r.passed ? '✅' : '❌';
    console.log(`  ${icon} ${r.name}: ${r.url} (${r.loadTime || 0}ms)`);
    if (!r.passed && r.error) console.log(`     error: ${r.error}`);
    if (r.passed) passed++;
    else failed++;
  }
  console.log(`\nTotal: ${passed} passed, ${failed} failed`);
  process.exit(failed > 0 ? 1 : 0);
}

main().catch(e => {
  console.error('FATAL:', e);
  process.exit(1);
});
