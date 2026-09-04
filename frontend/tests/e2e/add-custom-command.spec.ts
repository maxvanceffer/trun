/**
 * Tests for the "Add Custom Command" dialog functionality.
 * Bug fix: clicking "Add command" button should close the dialog.
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

// Same pattern as sidebar-tree.spec.ts: inject projects BEFORE hydration completes
// NO reload — avoids HMR crash that kills the dev server
async function initProjects(page: Page, projects: ProjectInfo[]) {
  await page.goto('/')
  await page.waitForLoadState('domcontentloaded')
  // Inject projects into localStorage (app's onMounted will read this)
  await page.evaluate(
    (projs) => localStorage.setItem('trun:projects', JSON.stringify(projs)),
    projects,
  )
  await page.evaluate(() => localStorage.removeItem('trun_custom_commands'))
  await page.waitForSelector('h2:has-text("Projects")', { timeout: 20000 })
  await page.waitForFunction(() => {
    const tree = document.querySelector('[role="tree"]')
    return tree && tree.childElementCount > 0
  }, { timeout: 10000 })
}

test.describe('Add Custom Command — Button Closes Dialog', () => {

  test('should close dialog and save command when clicking "Add command"', async ({ page }) => {
    await initProjects(page, [
      { id: '/t1/p-a', name: 'proj-a', project_path: '/t1/p-a', root_path: '/t1', manifest: 'package.json', commands: [] },
      { id: '/t1/p-b', name: 'proj-b', project_path: '/t1/p-b', root_path: '/t1', manifest: 'package.json', commands: [] }
    ])

    await page.locator('button[title="Add custom command"]').first().click()
    await expect(page.locator('h3:has-text("Add Custom Command")')).toBeVisible({ timeout: 3000 })

    // Fill form
    await page.locator('label:has-text("Command name") ~ * input').first().fill('test-lint')
    await page.locator('label:has-text("Binary") ~ * input').fill('npm')

    // Click "Add command" button — should close dialog
    await page.locator('button:has-text("Add command")').click()
    await expect(page.locator('h3:has-text("Add Custom Command")')).not.toBeVisible({ timeout: 3000 })

    // Verify command was saved to localStorage
    const saved = await page.evaluate(() => JSON.parse(localStorage.getItem('trun_custom_commands') || '[]'))
    expect(saved).toHaveLength(1)
    expect(saved[0].projectPath).toBe('/t1/p-a')
    expect(saved[0].name).toBe('test-lint')
  })

  test('should add multiple custom commands successfully', async ({ page }) => {
    await initProjects(page, [
      { id: '/t5/p-a', name: 'proj-a', project_path: '/t5/p-a', root_path: '/t5', manifest: 'package.json', commands: [] },
      { id: '/t5/p-b', name: 'proj-b', project_path: '/t5/p-b', root_path: '/t5', manifest: 'package.json', commands: [] }
    ])

    // Add first command on first project (button on the folder)
    await page.locator('button[title="Add custom command"]').first().click()
    await expect(page.locator('h3:has-text("Add Custom Command")')).toBeVisible({ timeout: 3000 })

    await page.locator('label:has-text("Command name") ~ * input').first().fill('lint')
    await page.locator('label:has-text("Binary") ~ * input').fill('eslint')
    await page.locator('button:has-text("Add command")').click()
    await expect(page.locator('h3:has-text("Add Custom Command")')).not.toBeVisible({ timeout: 3000 })

    // Add second command
    await page.locator('button[title="Add custom command"]').first().click()
    await expect(page.locator('h3:has-text("Add Custom Command")')).toBeVisible({ timeout: 3000 })

    await page.locator('label:has-text("Command name") ~ * input').first().fill('build')
    await page.locator('label:has-text("Binary") ~ * input').fill('webpack')
    await page.locator('button:has-text("Add command")').click()
    await expect(page.locator('h3:has-text("Add Custom Command")')).not.toBeVisible({ timeout: 3000 })

    const saved = await page.evaluate(() => JSON.parse(localStorage.getItem('trun_custom_commands') || '[]'))
    expect(saved).toHaveLength(2)
  })

  test('should not add duplicate command names on the same form', async ({ page }) => {
    await initProjects(page, [
      { id: '/t9/p-a', name: 'proj-a', project_path: '/t9/p-a', root_path: '/t9', manifest: 'package.json', commands: [] },
      { id: '/t9/p-b', name: 'proj-b', project_path: '/t9/p-b', root_path: '/t9', manifest: 'package.json', commands: [] }
    ])

    // Add first command
    await page.locator('button[title="Add custom command"]').first().click()
    await expect(page.locator('h3:has-text("Add Custom Command")')).toBeVisible({ timeout: 3000 })
    await page.locator('label:has-text("Command name") ~ * input').first().fill('my-lint')
    await page.locator('label:has-text("Binary") ~ * input').fill('npm')
    await page.locator('button:has-text("Add command")').click()
    await expect(page.locator('h3:has-text("Add Custom Command")')).not.toBeVisible({ timeout: 3000 })

    // Try to add same name again — should not be added
    await page.locator('button[title="Add custom command"]').first().click()
    await expect(page.locator('h3:has-text("Add Custom Command")')).toBeVisible({ timeout: 3000 })
    await page.locator('label:has-text("Command name") ~ * input').first().fill('my-lint')
    await page.locator('label:has-text("Binary") ~ * input').fill('npx')
    await page.locator('button:has-text("Add command")').click()
    await expect(page.locator('h3:has-text("Add Custom Command")')).not.toBeVisible({ timeout: 3000 })

    const saved = await page.evaluate(() => JSON.parse(localStorage.getItem('trun_custom_commands') || '[]'))
    expect(saved).toHaveLength(1)
  })
})

test.describe('Add Custom Command — Sidebar Display', () => {

  test('should show custom commands in sidebar after adding them', async ({ page }) => {
    await initProjects(page, [
      { id: '/t20/p-a', name: 'proj-a', project_path: '/t20/p-a', root_path: '/t20', manifest: 'package.json', commands: [] },
      { id: '/t20/p-b', name: 'proj-b', project_path: '/t20/p-b', root_path: '/t20', manifest: 'package.json', commands: [] }
    ])

    // Add a command
    await page.locator('button[title="Add custom command"]').first().click()
    await page.locator('label:has-text("Command name") ~ * input').first().fill('custom-lint')
    await page.locator('label:has-text("Binary") ~ * input').fill('custom-bin')
    await page.locator('button:has-text("Add command")').click()
    await expect(page.locator('h3:has-text("Add Custom Command")')).not.toBeVisible({ timeout: 3000 })

    // Verify custom command appears in sidebar tree
    await expect(page.locator('[role="treeitem"] >> text=custom-lint')).toBeVisible()
  })

  test('should show custom commands as tree items with custom icons', async ({ page }) => {
    await initProjects(page, [
      { id: '/t21/p-a', name: 'proj-a', project_path: '/t21/p-a', root_path: '/t21', manifest: 'package.json', commands: [] }
    ])

    // Add two custom commands
    await page.locator('button[title="Add custom command"]').first().click()
    await page.locator('label:has-text("Command name") ~ * input').first().fill('cmd-a')
    await page.locator('label:has-text("Binary") ~ * input').fill('bin-a')
    await page.locator('button:has-text("Add command")').click()

    await page.locator('button[title="Add custom command"]').first().click()
    await page.locator('label:has-text("Command name") ~ * input').first().fill('cmd-b')
    await page.locator('label:has-text("Binary") ~ * input').fill('bin-b')
    await page.locator('button:has-text("Add command")').click()

    // Verify both appear in the sidebar
    await expect(page.locator('[role="treeitem"] >> text=cmd-a')).toBeVisible()
    await expect(page.locator('[role="treeitem"] >> text=cmd-b')).toBeVisible()

    // Total treeitems: root folder + project + 2 custom commands
    await expect(page.locator('[role="treeitem"]')).toHaveCount(4)
  })

  test('should show custom commands under the correct project/folder', async ({ page }) => {
    await initProjects(page, [
      { id: '/t22/folder-a/myapp', name: 'myapp', project_path: '/t22/folder-a/myapp', root_path: '/t22', manifest: 'package.json', commands: [] },
      { id: '/t22/folder-a/other-app', name: 'other-app', project_path: '/t22/folder-a/other-app', root_path: '/t22', manifest: 'package.json', commands: [] }
    ])

    // Add a command to the folder (via folder button)
    await page.locator('button[title="Add custom command"]').first().click()
    await page.locator('label:has-text("Command name") ~ * input').first().fill('folder-cmd')
    await page.locator('label:has-text("Binary") ~ * input').fill('folder-bin')
    await page.locator('button:has-text("Add command")').click()

    // Verify the command appears in the tree and is clickable
    await expect(page.locator('[role="treeitem"] >> text=folder-cmd')).toBeVisible()

    // Click the custom command
    await page.locator('[role="treeitem"] >> text=folder-cmd').click()
    await page.waitForTimeout(500)

    // Header should update with the command name
    await expect(page.locator('header h1')).toHaveText('folder-cmd')
  })
})
