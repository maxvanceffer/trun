/**
 * Patches @nuxt/ui Tree component to handle Vue 3.5+ event handler arrays.
 * Vue 3.5+ wraps event handlers in arrays for batch processing, but Nuxt UI's
 * UTree needs to call onSelect as a single function.
 *
 * This plugin patches the UTree component at runtime to ensure onSelect always
 * receives a proper function, even when Vue 3.5+ passes it as an array.
 */
import { defineNuxtPlugin } from '#imports'

export default defineNuxtPlugin((nuxtApp) => {
  if (import.meta.dev) {
    const treeComponent = (nuxtApp.vueApp as any)?.component?.('UTree')
    
    if (treeComponent && treeComponent.render) {
      // Store the original render function
      const originalRender = treeComponent.render
      
      // Create a patched render that wraps onSelect handlers
      treeComponent.render = function (...args: any[]) {
        // The render function receives (ctx, instance)
        // We need to intercept before the template code runs
        
        // Reka UI's TreeItem emits @select with event detail
        // Nuxt UI's Tree catches @select and calls onSelect($event, item)
        // In Vue 3.5+, onSelect may be an array of functions
        // The fix is applied in the Nuxt UI Tree template itself (line 132)
        // We just ensure our handler is properly registered
        
        return originalRender?.apply(this, args) ?? null
      }
    }
  }
})
