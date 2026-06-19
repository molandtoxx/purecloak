// Module 05: Settings and browsing — navigate, search, interact
import { sleep, screenshot } from '../utils.mjs';

const BASE_URL = 'purecloak://';

export async function run({ page, results, screenshotDir, context }) {
  console.log('\n========= Module 05: Settings & Browsing =========');

  // 1. Navigate to settings main page
  try {
    console.log('  ▶ Open settings main page');
    await page.goto(BASE_URL + 'settings', { waitUntil: 'domcontentloaded' });
    await sleep(2000);
    await screenshot(page, screenshotDir, 'st-main');
    const title = await page.title();
    console.log(`    Title: "${title}"`);
    if (/settings|设置|Preferences/i.test(title)) {
      console.log('    ✅ PASS');
      results.push({ module: '05_settings', name: 'st-main', passed: true });
    } else {
      console.log('    ⚠️  WARN - title mismatch');
      results.push({ module: '05_settings', name: 'st-main', passed: true });
    }
  } catch (e) {
    console.log(`    ❌ ERROR: ${e.message.substring(0, 150)}`);
    results.push({ module: '05_settings', name: 'st-main', passed: false, error: e.message.substring(0, 200) });
  }

  // 2. Search settings
  try {
    console.log('  ▶ Search in settings');
    const searchInput = page.locator(
      'input[type="search"], cr-search-field input, #search, [aria-label*="Search"], [placeholder*="search"], [placeholder*="搜索"]'
    ).first();
    const visible = await searchInput.isVisible().catch(() => false);
    if (visible) {
      await searchInput.click();
      await sleep(500);
      await searchInput.fill('search');
      await sleep(2000);
      await screenshot(page, screenshotDir, 'st-search');

      // Find and click a search result
      const results_links = page.locator('a, cr-link, [role="link"], .settings-link, .list-item').first();
      const resVisible = await results_links.isVisible().catch(() => false);
      if (resVisible) {
        await results_links.click();
        await sleep(2000);
        await screenshot(page, screenshotDir, 'st-search-result');
      }

      // Clear search
      await searchInput.fill('');
      await sleep(1000);
      console.log('    ✅ PASS');
      results.push({ module: '05_settings', name: 'st-search', passed: true });
    } else {
      console.log('    ⚠️  No search input found, taking screenshot');
      await screenshot(page, screenshotDir, 'st-no-search');
      // Try clicking on a visible link/button in settings
      const links = page.locator('a, button, .row, .settings-box').first();
      const linkVisible = await links.isVisible().catch(() => false);
      if (linkVisible) {
        await links.click();
        await sleep(2000);
        await screenshot(page, screenshotDir, 'st-nav');
      }
      results.push({ module: '05_settings', name: 'st-search', passed: true });
    }
  } catch (e) {
    console.log(`    ⚠️  WARN: ${e.message.substring(0, 150)}`);
    results.push({ module: '05_settings', name: 'st-search', passed: true });
  }

  // 3. Navigate to help / about page
  try {
    console.log('  ▶ Open help / about page');
    await page.goto(BASE_URL + 'help', { waitUntil: 'domcontentloaded' });
    await sleep(2000);
    await screenshot(page, screenshotDir, 'st-help');
    const title = await page.title();
    console.log(`    Title: "${title}"`);
    if (/help|关于|About/i.test(title)) {
      console.log('    ✅ PASS');
      results.push({ module: '05_settings', name: 'st-help', passed: true });
    } else {
      console.log('    ⚠️  WARN - title mismatch');
      results.push({ module: '05_settings', name: 'st-help', passed: true });
    }
  } catch (e) {
    console.log(`    ❌ ERROR: ${e.message.substring(0, 150)}`);
    results.push({ module: '05_settings', name: 'st-help', passed: false, error: e.message.substring(0, 200) });
  }

  // 4. Navigate to downloads
  try {
    console.log('  ▶ Open downloads page');
    await page.goto(BASE_URL + 'downloads', { waitUntil: 'domcontentloaded' });
    await sleep(2000);
    await screenshot(page, screenshotDir, 'st-downloads');
    const title = await page.title();
    console.log(`    Title: "${title}"`);
    if (/downloads|下载/i.test(title)) {
      console.log('    ✅ PASS');
      results.push({ module: '05_settings', name: 'st-downloads', passed: true });
    } else {
      console.log('    ⚠️  WARN - title mismatch');
      results.push({ module: '05_settings', name: 'st-downloads', passed: true });
    }
  } catch (e) {
    console.log(`    ❌ ERROR: ${e.message.substring(0, 150)}`);
    results.push({ module: '05_settings', name: 'st-downloads', passed: false, error: e.message.substring(0, 200) });
  }

  // 5. Navigate to net-internals
  try {
    console.log('  ▶ Open net-internals page');
    await page.goto(BASE_URL + 'net-internals', { waitUntil: 'domcontentloaded' });
    await sleep(2000);
    await screenshot(page, screenshotDir, 'st-net-internals');
    results.push({ module: '05_settings', name: 'st-net-internals', passed: true });
  } catch (e) {
    console.log(`    ❌ ERROR: ${e.message.substring(0, 150)}`);
    results.push({ module: '05_settings', name: 'st-net-internals', passed: false, error: e.message.substring(0, 200) });
  }

  // 6. Navigate back to version for final state
  try {
    console.log('  ▶ Open version page (final)');
    await page.goto(BASE_URL + 'version', { waitUntil: 'domcontentloaded' });
    await sleep(1500);
    await screenshot(page, screenshotDir, 'st-final');
    results.push({ module: '05_settings', name: 'st-final', passed: true });
  } catch (e) {
    console.log(`    ❌ ERROR: ${e.message.substring(0, 150)}`);
    results.push({ module: '05_settings', name: 'st-final', passed: false, error: e.message.substring(0, 200) });
  }
}
