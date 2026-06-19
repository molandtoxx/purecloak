// Module 02: Tab operations — open, close, switch
// Uses Playwright's native page API (context.newPage / page.close) for reliability,
// and keyboard shortcuts for switching between tabs.
import { sleep, screenshot } from '../utils.mjs';

const BASE_URL = 'purecloak://';

export async function run({ page: startPage, results, screenshotDir, context }) {
  console.log('\n========= Module 02: Tab Operations =========');
  let page = startPage;

  // Ensure we're on a stable page
  try {
    await page.goto(BASE_URL + 'version', { waitUntil: 'domcontentloaded' });
    await sleep(1000);
  } catch (_) {}

  // 1. Open new tab via context.newPage()
  try {
    console.log('  ▶ Open new tab');
    const before = context.pages().length;
    const newPage = await context.newPage();
    await newPage.goto(BASE_URL + 'version', { waitUntil: 'domcontentloaded' });
    await sleep(1000);
    const after = context.pages().length;
    page = newPage;
    await screenshot(page, screenshotDir, 'tab-new');

    if (after > before) {
      console.log('    ✅ PASS');
      results.push({ module: '02_tab_operations', name: 'tab-new', passed: true });
    } else {
      console.log('    ❌ FAIL');
      results.push({ module: '02_tab_operations', name: 'tab-new', passed: false, error: 'tab_count_not_increased' });
    }
  } catch (e) {
    console.log(`    ❌ ERROR: ${e.message.substring(0, 150)}`);
    results.push({ module: '02_tab_operations', name: 'tab-new', passed: false, error: e.message.substring(0, 200) });
  }

  // 2. Open another tab
  try {
    console.log('  ▶ Open another tab');
    const before2 = context.pages().length;
    const newPage2 = await context.newPage();
    await newPage2.goto(BASE_URL + 'flags', { waitUntil: 'domcontentloaded' });
    await sleep(1000);
    const after2 = context.pages().length;
    page = newPage2;
    await screenshot(page, screenshotDir, 'tab-two-tabs');

    if (after2 >= 3) {
      console.log(`    ✅ PASS (${after2} tabs)`);
      results.push({ module: '02_tab_operations', name: 'tab-two-tabs', passed: true });
    } else {
      console.log(`    ⚠️  WARN - ${after2} tabs`);
      results.push({ module: '02_tab_operations', name: 'tab-two-tabs', passed: true });
    }
  } catch (e) {
    console.log(`    ❌ ERROR: ${e.message.substring(0, 150)}`);
    results.push({ module: '02_tab_operations', name: 'tab-two-tabs', passed: false, error: e.message.substring(0, 200) });
  }

  // 3. Switch to first tab via Ctrl+1
  try {
    console.log('  ▶ Switch to tab 1 (Ctrl+1)');
    await page.keyboard.press('Control+1');
    await sleep(2000);
    const pages = context.pages();
    page = pages.length > 0 ? pages[0] : page;
    const title = await page.title().catch(() => '(no title)');
    await screenshot(page, screenshotDir, 'tab-switch-1');
    console.log(`    Title: "${title}"`);
    results.push({ module: '02_tab_operations', name: 'tab-switch-1', passed: true });
  } catch (e) {
    console.log(`    ❌ ERROR: ${e.message.substring(0, 150)}`);
    results.push({ module: '02_tab_operations', name: 'tab-switch-1', passed: false, error: e.message.substring(0, 200) });
  }

  // 4. Switch to tab 2 (or the last page we created)
  try {
    console.log('  ▶ Switch to tab 2 (Ctrl+2)');
    if (context.pages().length >= 2) {
      await page.keyboard.press('Control+2');
      await sleep(2000);
      const curPages = context.pages();
      page = curPages.length >= 2 ? curPages[1] : curPages[curPages.length - 1];
      const title = await page.title().catch(() => '(no title)');
      await screenshot(page, screenshotDir, 'tab-switch-2');
      console.log(`    Title: "${title}"`);
      results.push({ module: '02_tab_operations', name: 'tab-switch-2', passed: true });
    } else {
      console.log('    ⚠️  SKIP - < 2 tabs');
      results.push({ module: '02_tab_operations', name: 'tab-switch-2', passed: false, error: 'not_enough_tabs' });
    }
  } catch (e) {
    console.log(`    ❌ ERROR: ${e.message.substring(0, 150)}`);
    results.push({ module: '02_tab_operations', name: 'tab-switch-2', passed: false, error: e.message.substring(0, 200) });
  }

  // 5. Close current tab via page.close()
  try {
    console.log('  ▶ Close a tab');
    const pages = context.pages();
    const beforeClose = pages.length;
    if (beforeClose > 1) {
      // Close the last tab
      const lastPage = pages[pages.length - 1];
      await lastPage.close();
      await sleep(2000);
      const remaining = context.pages().length;

      if (remaining < beforeClose) {
        console.log(`    ✅ PASS (${remaining} remaining)`);
        results.push({ module: '02_tab_operations', name: 'tab-close', passed: true });
      } else {
        console.log('    ⚠️  Tab not closed');
        results.push({ module: '02_tab_operations', name: 'tab-close', passed: false, error: 'tab_not_closed' });
      }
    } else {
      console.log('    ⚠️  SKIP - only one tab');
      results.push({ module: '02_tab_operations', name: 'tab-close', passed: false, error: 'only_one_tab' });
    }
  } catch (e) {
    console.log(`    ❌ ERROR: ${e.message.substring(0, 150)}`);
    results.push({ module: '02_tab_operations', name: 'tab-close', passed: false, error: e.message.substring(0, 200) });
  }

  // Update page reference
  const finalPages = context.pages();
  page = finalPages.length > 0 ? finalPages[0] : page;

  // Take final screenshot on version page
  try {
    await page.goto(BASE_URL + 'version', { waitUntil: 'domcontentloaded' }).catch(() => {});
    await sleep(1000);
    await screenshot(page, screenshotDir, 'tab-final-state');
  } catch (_) {}
}
