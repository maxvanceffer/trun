#!/usr/bin/env node
/**
 * Postinstall script that patches @nuxt/ui Tree component to handle Vue 3.5+
 * event handler arrays. Vue 3.5+ wraps event handlers in arrays for batch
 * processing, but Nuxt UI's UTree tries to call them directly.
 *
 * This ensures the fix persists across npm install operations.
 */
import { readFileSync, writeFileSync, existsSync } from 'fs'
import { fileURLToPath } from 'url'
import { dirname, join } from 'path'

const __filename = fileURLToPath(import.meta.url)
const __dirname = dirname(__filename)

const treePath = join(__dirname, '../node_modules/@nuxt/ui/dist/runtime/components/Tree.vue')

if (!existsSync(treePath)) {
  console.log('⚠️  Tree.vue not found, skipping patch')
  process.exit(0)
}

const original = readFileSync(treePath, 'utf8')

// Check if already patched
if (original.includes('Array.isArray(fn)')) {
  console.log('✅ Tree.vue already patched')
  process.exit(0)
}

// Patch the component - handle both @select and @toggle events
const patched = original
  .replace(
    /@select="(\(\s*[^"]*props\.onSelect[^"]*?\)\??\([^)]*\))"/g,
    '@select="(() => { const fn = item.onSelect ?? props.onSelect; if (Array.isArray(fn)) fn.forEach(f => f?.($event, item)); else fn?.($event, item); })()"'
  )
  .replace(
    /@toggle="(\(\s*[^"]*props\.onToggle[^"]*?\)\??\([^)]*\))"/g,
    '@toggle="(() => { const fn = item.onToggle ?? props.onToggle; if (Array.isArray(fn)) fn.forEach(f => f?.($event, item)); else fn?.($event, item); })()"'
  )

if (patched !== original) {
  writeFileSync(treePath, patched, 'utf8')
  console.log('✅ Patched Tree.vue to handle Vue 3.5+ event handler arrays')
} else {
  console.log('⚠️  Tree.vue patch had no effect (pattern not matched)')
}
