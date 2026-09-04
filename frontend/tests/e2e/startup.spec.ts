import { test, expect } from '@playwright/test'

test.describe('Startup behavior (frontend only)', () => {
  // Note: Full startup window logic (hide all → check projects → show window)
  // is tested in Rust unit tests (lib.rs tests). Playwright tests the frontend
  // route behavior in isolation since Playwright hits localhost:3000 directly
  // without going through Tauri's window management.

  test.beforeEach(async ({ page }) => {
    // Navigate to the dashboard fresh
    await page.goto('/')
    // Clear any existing project data
    await page.evaluate(() => localStorage.removeItem('trun:projects'))
    // Wait for the "No projects found" text to be accessible (hydration complete)
    await page.waitForSelector('text=No projects found', { timeout: 10000 })
  })

  test('fresh start: no auto-navigate, shows dashboard with no projects', async ({
    page,
  }) => {
    // Already verified in beforeEach that we land on / with "No projects found"
    await expect(page).toHaveURL('/')
  })

  test('after scan flow: dashboard shows projects from localStorage', async ({
    page,
  }) => {
    // Simulate projects saved by cmd_scan event handler (after a real scan)
    // Note: Project IDs use the project_path format (no colon suffix)
    // so they appear as flat tree items, not folder items.
    const testProjects = [
      {
        id: '/tmp/hello',
        name: 'Hello World',
        project_path: '/tmp/hello',
        root_path: '/tmp',
        icon: 'ts',
        manifest: 'package.json',
        commands: [
          {
            category: 'npm',
            label: 'dev',
            command: 'npm',
            args: ['run', 'dev'],
          },
        ],
      },
    ]

    // Save projects to localStorage (simulating post-scan state)
    await page.evaluate(
      (projects) => localStorage.setItem('trun:projects', JSON.stringify(projects)),
      testProjects,
    )

    // Reload page to trigger the onMounted load from localStorage
    await page.reload({ waitUntil: 'domcontentloaded' })

    // Wait briefly for the tree to build
    await page.waitForTimeout(500)

    // Project should be visible in the tree
    await expect(page.getByText('Hello World')).toBeVisible({ timeout: 5000 })
  })
})
