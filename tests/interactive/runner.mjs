// Interactive UI Test Runner for PureCloak
// Runs all test modules in headed mode with screenshots
import { chromium } from 'playwright';
import fs from 'fs';
import path from 'path';

const CHROME_BIN = '/home/molandtoxx/PureCloak/chromium_src/out/purecloak/chrome';
const BASE_URL = 'purecloak://';
const USER_DATA_DIR = '/tmp/pc-interactive-' + Date.now();
const SCREENSHOT_DIR = '/tmp/pc-interactive-screenshots/' + Date.now();
const LOCAL_STATE_PATH = path.join(USER_DATA_DIR, 'Local State');

// Ensure directories exist
fs.mkdirSync(SCREENSHOT_DIR, { recursive: true });
fs.mkdirSync(USER_DATA_DIR, { recursive: true });

// Write Local State pref to enable internal debug pages
fs.writeFileSync(LOCAL_STATE_PATH, JSON.stringify({
  internal_only_uis_enabled: true,
}));

async function runModule(name, importFn) {
  console.log(`\n${'='.repeat(60)}`);
  console.log(`Loading module: ${name}`);
  console.log(`${'='.repeat(60)}`);
  const mod = await importFn();
  return mod.run;
}

async function main() {
  console.log('='.repeat(60));
  console.log('  PureCloak Interactive UI Test Suite');
  console.log('  Screenshots: ' + SCREENSHOT_DIR);
  console.log('='.repeat(60));

  console.log('\nLaunching browser (headed mode)...');
  const context = await chromium.launchPersistentContext(USER_DATA_DIR, {
    executablePath: CHROME_BIN,
    headless: false,
    args: [
      '--no-first-run',
      '--disable-fre',
      '--no-default-browser-check',
      '--disable-search-engine-choice-screen',
      '--window-size=1280,800',
    ],
    ignoreHTTPSErrors: true,
  });

  const allResults = [];
  const modules = [
    { name: '01_main_menu', path: './modules/01_main_menu.mjs' },
    { name: '02_tab_operations', path: './modules/02_tab_operations.mjs' },
    { name: '03_context_menus', path: './modules/03_context_menus.mjs' },
    { name: '04_bookmarks', path: './modules/04_bookmarks.mjs' },
    { name: '05_settings_browsing', path: './modules/05_settings_browsing.mjs' },
  ];

  let currentPage = null;

  for (const mod of modules) {
    try {
      const runFn = await runModule(mod.name, () => import(mod.path));
      // Reset: close extra pages but keep at least one alive
      // (closing the last page in persistent mode closes the context)
      const allPages = context.pages();
      for (let i = 1; i < allPages.length; i++) {
        try { await allPages[i].close().catch(() => {}); } catch {}
      }
      await new Promise(r => setTimeout(r, 500));
      // Reuse the first page if it exists, otherwise create a new one
      const remaining = context.pages();
      if (remaining.length > 0) {
        currentPage = remaining[0];
        // Navigate to a safe page
        try { await currentPage.goto('purecloak://version', { waitUntil: 'domcontentloaded', timeout: 10000 }).catch(() => {}); } catch {}
      } else {
        currentPage = await context.newPage();
      }

      const sharedState = {
        page: currentPage,
        results: allResults,
        screenshotDir: SCREENSHOT_DIR,
        context,
        BASE_URL,
      };

      await runFn(sharedState);

      // Small pause between modules
      await new Promise(r => setTimeout(r, 1000));
    } catch (e) {
      console.error(`\n❌ Module ${mod.name} crashed: ${e.message}`);
      allResults.push({ module: mod.name, name: 'module-load', passed: false, error: e.message.substring(0, 200) });
    }
  }

  // Close browser
  try {
    await context.close();
  } catch (_) {}

  // Print summary
  console.log('\n' + '='.repeat(60));
  console.log('  FINAL RESULTS');
  console.log('='.repeat(60));

  const passed = allResults.filter(r => r.passed).length;
  const failed = allResults.filter(r => !r.passed).length;

  // Group by module
  const byModule = {};
  for (const r of allResults) {
    if (!byModule[r.module]) byModule[r.module] = [];
    byModule[r.module].push(r);
  }

  for (const [modName, modResults] of Object.entries(byModule)) {
    const modPassed = modResults.filter(r => r.passed).length;
    const modFailed = modResults.filter(r => !r.passed).length;
    console.log(`\n  ${modName}: ${modPassed} ✅, ${modFailed} ❌`);
    for (const r of modResults) {
      if (r.passed) {
        console.log(`    ✅ ${r.name}`);
      } else {
        console.log(`    ❌ ${r.name}${r.error ? ': ' + r.error.substring(0, 120) : ''}`);
      }
    }
  }

  console.log(`\n  Total: ${passed} passed, ${failed} failed`);
  console.log(`  Screenshots: ${SCREENSHOT_DIR}`);
  console.log('='.repeat(60));

  process.exit(failed > 0 ? 1 : 0);
}

main().catch(e => {
  console.error('FATAL:', e);
  process.exit(1);
});
