/**
 * Tests for the sidebar tree view selection behavior.
 * Each test injects project data via localStorage before navigating.
 */
import { test, expect, type Page } from '@playwright/test'

interface ProjectInfo {
  id: string
  name: string
  project_path: string
  root_path: string
  manifest: string
  commands: Array<{ id: string; category: string; label: string; command: string; args: string[] }>
}

function injectProjects(page: Page, projects: ProjectInfo[]) {
  return page.evaluate((projs) => localStorage.setItem('trun:projects', JSON.stringify(projs)), projects)
}

// Helper: go first, inject projects, then wait for hydration (NO reload to avoid HMR crash)
async function gotoWithProjects(page: Page, projects: ProjectInfo[]) {
  await page.goto('/')
  await page.waitForLoadState('domcontentloaded')
  await injectProjects(page, projects)
  await page.evaluate(() => localStorage.removeItem('trun_custom_commands'))
  // Instead of reload, wait for the app to pick up the data
  await page.waitForSelector('h2:has-text("Projects")', { timeout: 10000 })
  await page.waitForFunction(() => {
    const tree = document.querySelector('[role="tree"]')
    return tree && tree.childElementCount > 0
  }, { timeout: 5000 })
}

test.describe('Sidebar Tree View', () => {
  test('should update content area when clicking a tree item', async ({ page }) => {
    // root_path === project_path -> no folder wrapper, single flat project = 1 treeitem
    await gotoWithProjects(page, [
      { id: '/tmp/myapp', name: 'myapp', project_path: '/tmp/myapp', root_path: '/tmp/myapp', manifest: 'package.json', commands: [] }
    ])
    await expect(page.locator('[role="treeitem"]')).toHaveCount(1)
    await page.locator('[role="treeitem"]').first().click()
    await page.waitForTimeout(500)
    await expect(page.locator('header h1')).toHaveText('myapp')
  })

  test('tree select works with multiple projects in same root', async ({ page }) => {
    // 2 projects, same root_path = folder with 2 children = 3 treeitems, 1 button on folder
    await gotoWithProjects(page, [
      { id: '/t2/p-a', name: 'proj-a', project_path: '/t2', root_path: '/t2', manifest: 'package.json', commands: [] },
      { id: '/t2/p-b', name: 'proj-b', project_path: '/t2', root_path: '/t2', manifest: 'package.json', commands: [] }
    ])
    await expect(page.locator('[role="treeitem"]')).toHaveCount(3) // folder + 2 children
    await page.locator('[role="treeitem"]').nth(1).click()
    await page.waitForTimeout(500)
    await expect(page.locator('header h1')).toHaveText(/proj-a|proj-b/)
  })
})

test.describe('Add Custom Command Button on Folders', () => {
  test('should show "+" button on folder items that have children', async ({ page }) => {
    // 3 projects at same dir -> 1 folder with 3 children = 4 treeitems, 1 button
    await gotoWithProjects(page, [
      { id: '/t3/p-a', name: 'proj-a', project_path: '/t3', root_path: '/t3', manifest: 'package.json', commands: [] },
      { id: '/t3/p-b', name: 'proj-b', project_path: '/t3', root_path: '/t3', manifest: 'package.json', commands: [] },
      { id: '/t3/p-c', name: 'proj-c', project_path: '/t3', root_path: '/t3', manifest: 'package.json', commands: [] }
    ])
    await expect(page.locator('[role="treeitem"]')).toHaveCount(4) // folder + 3 children
    await expect(page.locator('button[title="Add custom command"]')).toHaveCount(1)
  })

  test('should NOT show "+" button on leaf project items', async ({ page }) => {
    // Each project at a separate root -> all flat items, no folders, no buttons
    await gotoWithProjects(page, [
      { id: '/t4a/p-a', name: 'proj-a', project_path: '/t4a/p-a', root_path: '/t4a/p-a', manifest: 'package.json', commands: [] },
      { id: '/t4b/p-b', name: 'proj-b', project_path: '/t4b/p-b', root_path: '/t4b/p-b', manifest: 'package.json', commands: [] },
      { id: '/t4c/p-c', name: 'proj-c', project_path: '/t4c/p-c', root_path: '/t4c/p-c', manifest: 'package.json', commands: [] }
    ])
    await expect(page.locator('button[title="Add custom command"]')).toHaveCount(0)
  })

  test('should show "+" button on nested folder items at all levels', async ({ page }) => {
    // /t5 (folder with children) + /t5/folder-a (folder with children)
    // /t5/folder-b has 1 project → also a folder (1 proj at dir, not single at dir)
    // = 3 folders with buttons
    await gotoWithProjects(page, [
      { id: '/t5/folder-a/p-a', name: 'p-a', project_path: '/t5/folder-a', root_path: '/t5', manifest: 'package.json', commands: [] },
      { id: '/t5/folder-a/p-b', name: 'p-b', project_path: '/t5/folder-a', root_path: '/t5', manifest: 'package.json', commands: [] },
      { id: '/t5/folder-b/p-c', name: 'p-c', project_path: '/t5/folder-b', root_path: '/t5', manifest: 'package.json', commands: [] }
    ])
    await expect(page.locator('button[title="Add custom command"]')).toHaveCount(3)
  })

  test('should show "+" button on isMultiManifestFolder items', async ({ page }) => {
    // 2 projects with manifest suffix at same dir -> /t6 (folder) + /t6/api (folder) + api proj + /t6/web (folder) + web proj = 5 treeitems
    // 3 buttons: /t6, /t6/api, /t6/web
    await gotoWithProjects(page, [
      { id: '/t6/api:package.json', name: 'api', project_path: '/t6', root_path: '/t6', manifest: 'package.json', commands: [] },
      { id: '/t6/web:package.json', name: 'web', project_path: '/t6', root_path: '/t6', manifest: 'package.json', commands: [] }
    ])
    await expect(page.locator('[role="treeitem"]')).toHaveCount(5)
    await expect(page.locator('button[title="Add custom command"]')).toHaveCount(3)
  })

  test('tree select works with multiple projects in different roots', async ({ page }) => {
    // /t7 (folder, 1 btn) + folder-a (folder, 1 btn, 2 children) + folder-b (folder, 1 btn, 2 children) = 7 treeitems, 3 buttons
    await gotoWithProjects(page, [
      { id: '/t7/folder-a/p-a', name: 'p-a', project_path: '/t7/folder-a', root_path: '/t7', manifest: 'package.json', commands: [] },
      { id: '/t7/folder-a/p-b', name: 'p-b', project_path: '/t7/folder-a', root_path: '/t7', manifest: 'package.json', commands: [] },
      { id: '/t7/folder-b/p-c', name: 'p-c', project_path: '/t7/folder-b', root_path: '/t7', manifest: 'package.json', commands: [] },
      { id: '/t7/folder-b/p-d', name: 'p-d', project_path: '/t7/folder-b', root_path: '/t7', manifest: 'package.json', commands: [] }
    ])
    await expect(page.locator('[role="treeitem"]')).toHaveCount(7)
    await expect(page.locator('button[title="Add custom command"]')).toHaveCount(3)
    await page.locator('button[title="Add custom command"]').last().click()
    await page.waitForTimeout(300)
    await expect(page.locator('h3:has-text("Add Custom Command")')).toBeVisible()
  })

  test('tree renders only leaf projects when there are no folders', async ({ page }) => {
    // 2 projects at same dir -> 1 folder + 2 children = 3 treeitems, 1 button
    await gotoWithProjects(page, [
      { id: '/t8/p-a', name: 'proj-a', project_path: '/t8', root_path: '/t8', manifest: 'package.json', commands: [] },
      { id: '/t8/p-b', name: 'proj-b', project_path: '/t8', root_path: '/t8', manifest: 'package.json', commands: [] }
    ])
    await expect(page.locator('[role="treeitem"]')).toHaveCount(3) // folder + 2 children
    await expect(page.locator('button[title="Add custom command"]')).toHaveCount(1)
  })
})
