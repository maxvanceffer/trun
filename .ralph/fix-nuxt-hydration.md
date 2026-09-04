## Dashboard Hydration & UTree onSelect Fix

### Problem
The dashboard page appeared blank because Vue 3.5+ wraps event handlers in arrays for batch processing, but Nuxt UI's UTree component tried to call them directly: `props.onSelect?.($event, item)`. When `onSelect` was an array, calling it as a function caused a silent crash.

### Root Cause
Vue 3.5+ changed how event handlers are stored internally. Instead of a single function, event handlers are now stored as arrays `[handler1, handler2]` to support batch processing. When Nuxt UI's UTree component tried to call `props.onSelect?.($event, item)`, it failed because arrays are not callable.

### Fix Applied
1. **Patched `@nuxt/ui/dist/runtime/components/Tree.vue`** to handle both arrays and functions:
   ```js
   // Before:
   @select="(item.onSelect ?? props.onSelect)?.($event, item)"
   
   // After:
   @select="(() => { const fn = item.onSelect ?? props.onSelect; if (Array.isArray(fn)) fn.forEach(f => f?.($event, item)); else fn?.($event, item); })()"
   ```
   
2. **Created durable fix** via `scripts/postinstall.mjs` that automatically patches the component after every `npm install`:
   - Checks if Tree.vue needs patching
   - Applies the fix if not already patched
   - Runs automatically in the `postinstall` hook

3. **Created Nuxt plugin** at `app/plugins/patch-utree.ts` as a backup runtime patch

### Verification
- ✅ No more `Invalid prop: type check failed for prop "onSelect". Expected Function, got Array` warnings
- ✅ Dashboard renders correctly with sidebar, tree, and main content area
- ✅ Activity log displays welcome message
- ✅ CPU/RAM stats display correctly
- ✅ Fix is durable across rebuilds via postinstall script

### Files Changed
- `frontend/node_modules/@nuxt/ui/dist/runtime/components/Tree.vue` (patched)
- `frontend/scripts/postinstall.mjs` (new - auto-patches on install)
- `frontend/app/plugins/patch-utree.ts` (new - runtime patch backup)
- `frontend/package.json` (updated postinstall hook)

### Status
✅ FIX COMPLETE - Dashboard hydration issue resolved, UTree selection validation working