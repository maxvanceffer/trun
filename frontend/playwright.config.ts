import { defineConfig, devices } from '@playwright/test'

export default defineConfig({
  testDir: './tests/e2e',
  timeout: 120_000,
  expect: {
    timeout: 10_000
  },
  use: {
    baseURL: 'http://localhost:3000',
    headless: true,
    screenshot: 'only-on-failure',
    video: 'retain-on-failure',
    trace: 'retain-on-failure',
    actionTimeout: 5000,
    navigationTimeout: 30000
  },
  projects: [
    {
      name: 'chromium',
      use: { ...devices['Desktop Chrome'] }
    }
  ],
  retries: 1, // HMR instability causes occasional false failures
  webServer: {
    command: 'npx nuxi dev --port 3000',
    url: 'http://localhost:3000',
    reuseExistingServer: true,
    timeout: 120_000,
    stderr: 'pipe'
  }
})
