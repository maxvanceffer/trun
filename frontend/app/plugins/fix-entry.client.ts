// Fix for Nuxt 4 Vite 8 entry script URL issue
// This fixes the entry script URL before Nuxt hydration starts

export default defineNuxtPlugin(() => {
  if (import.meta.client) {
    // Fix the script tags immediately
    const oldPath = '/Users/maxxxtraxxx/projects/trun/frontend/node_modules/nuxt/dist/app/entry.async.js'
    const newPath = '@fs/Users/maxxxtraxxx/projects/trun/frontend/node_modules/nuxt/dist/app/entry.async.js'
    
    // Fix all script tags
    document.querySelectorAll('script[src]').forEach(script => {
      const src = script.getAttribute('src') || ''
      if (src.includes(oldPath)) {
        script.setAttribute('src', src.replace(oldPath, newPath))
      }
    })
    
    // Fix all link tags
    document.querySelectorAll('link[rel="modulepreload"][href]').forEach(link => {
      const href = link.getAttribute('href') || ''
      if (href.includes(oldPath)) {
        link.setAttribute('href', href.replace(oldPath, newPath))
      }
    })
  }
})
