// Shared utilities for interactive test suite
import path from 'path';

export function sleep(ms) {
  return new Promise(r => setTimeout(r, ms));
}

export async function screenshot(page, dir, name) {
  try {
    const timestamp = Date.now();
    const filePath = path.join(dir, `${timestamp}-${name}.png`);
    await page.screenshot({ path: filePath, fullPage: false });
    console.log(`    📸 Screenshot: ${name}.png`);
  } catch (e) {
    console.log(`    ⚠️  Screenshot failed: ${e.message.substring(0, 80)}`);
  }
}

export async function expectVisible(page, selector, timeout = 3000) {
  try {
    await page.waitForSelector(selector, { state: 'visible', timeout });
    return true;
  } catch {
    return false;
  }
}

export async function getPageTitle(page) {
  try {
    return await page.title();
  } catch {
    return '';
  }
}
