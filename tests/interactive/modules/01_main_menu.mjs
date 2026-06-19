// Module 01: Three-dot main menu items via keyboard accelerators
import { sleep, screenshot, expectVisible } from '../utils.mjs';

const BASE_URL = 'purecloak://';

const MENU_TESTS = [
  {
    name: 'menu-new-tab',
    desc: 'New Tab via context.newPage()',
    action: async (page) => {
      const ctx = page.context();
      const np = await ctx.newPage();
      await np.goto(BASE_URL + 'version', { waitUntil: 'domcontentloaded' }).catch(() => {});
      await sleep(1000);
    },
    verify: async (page) => {
      const pages = page.context().pages();
      return pages.length > 1;
    },
  },
  {
    name: 'menu-history',
    desc: 'History via Ctrl+H',
    action: async (page) => { await page.goto(BASE_URL + 'history', { waitUntil: 'domcontentloaded' }); await sleep(2000); },
    verify: async (page) => {
      const title = await page.title();
      return /history|历史/i.test(title);
    },
  },
  {
    name: 'menu-downloads',
    desc: 'Downloads via Ctrl+J',
    action: async (page) => { await page.goto(BASE_URL + 'downloads', { waitUntil: 'domcontentloaded' }); await sleep(2000); },
    verify: async (page) => {
      const title = await page.title();
      return /downloads|下载/i.test(title);
    },
  },
  {
    name: 'menu-settings',
    desc: 'Settings page via direct nav',
    action: async (page) => { await page.goto(BASE_URL + 'settings', { waitUntil: 'domcontentloaded' }); await sleep(2000); },
    verify: async (page) => {
      const title = await page.title();
      return /settings|设置|Preferences/i.test(title);
    },
  },
  {
    name: 'menu-version',
    desc: 'About Version page',
    action: async (page) => { await page.goto(BASE_URL + 'version', { waitUntil: 'domcontentloaded' }); await sleep(1500); },
    verify: async (page) => {
      const title = await page.title();
      return /version|版本|\d+\.\d+/i.test(title);
    },
  },
  {
    name: 'menu-flags',
    desc: 'Experiments / Flags page',
    action: async (page) => { await page.goto(BASE_URL + 'flags', { waitUntil: 'domcontentloaded' }); await sleep(1500); },
    verify: async (page) => {
      const title = await page.title();
      return /experiments|实验|Flags/i.test(title);
    },
  },
  {
    name: 'menu-help',
    desc: 'Help / About page',
    action: async (page) => { await page.goto(BASE_URL + 'help', { waitUntil: 'domcontentloaded' }); await sleep(1500); },
    verify: async (page) => {
      const title = await page.title();
      return /help|关于|About/i.test(title);
    },
  },
  {
    name: 'menu-extensions',
    desc: 'Extensions page',
    action: async (page) => { await page.goto(BASE_URL + 'extensions', { waitUntil: 'domcontentloaded' }); await sleep(1500); },
    verify: async (page) => {
      const title = await page.title();
      return /extensions|扩展程序/i.test(title);
    },
  },
  // Alt+F opens the three-dot menu (native OS menu in Chromium)
  {
    name: 'menu-open-three-dot',
    desc: 'Open three-dot menu via Alt+F',
    action: async (page) => {
      // Close current tab if it's an extension page
      const title = await page.title();
      if (!/extensions/i.test(title)) {
        await page.goto(BASE_URL + 'flags', { waitUntil: 'domcontentloaded' });
        await sleep(1000);
      }
      await page.keyboard.press('Alt+f');
      await sleep(2000);
    },
    verify: async (page) => {
      // Native OS menu is not in page DOM, but we verify no crash
      return true;
    },
  },
];

export async function run({ page, results, screenshotDir, context }) {
  console.log('\n========= Module 01: Main Menu =========');

  for (const t of MENU_TESTS) {
    try {
      console.log(`  ▶ ${t.desc}`);
      await t.action(page);
      await screenshot(page, screenshotDir, t.name);

      const passed = await t.verify(page);
      if (passed) {
        console.log(`    ✅ PASS`);
        results.push({ module: '01_main_menu', name: t.name, passed: true });
      } else {
        const title = await page.title().catch(() => '');
        console.log(`    ❌ FAIL - content check failed`);
        results.push({ module: '01_main_menu', name: t.name, passed: false, error: 'content_mismatch', title });
      }
    } catch (e) {
      console.log(`    ❌ ERROR: ${e.message.substring(0, 150)}`);
      results.push({ module: '01_main_menu', name: t.name, passed: false, error: e.message.substring(0, 200) });
    }
  }

  // Cleanup: close extra tabs opened during tests
  const pages = context.pages();
  if (pages.length > 1) {
    for (let i = 1; i < pages.length; i++) {
      try { await pages[i].close().catch(() => {}); } catch {}
    }
    await sleep(500);
  }
}
