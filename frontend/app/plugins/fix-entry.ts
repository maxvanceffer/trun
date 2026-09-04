// Fix for Nuxt 4 Vite 8 entry script URL issue
// The entry script URL is missing the @fs prefix which causes a 404

export default defineNuxtPlugin((nuxtApp) => {
  if (import.meta.client) {
    nuxtApp.hook('app:created', () => {
      // Fix the entry script URL by adding @fs prefix
      const scripts = document.querySelectorAll('script[src*="/_nuxt/Users/maxxxtraxxx/projects/trun/frontend/node_modules/nuxt/dist/app/entry.async.js"]')
      scripts.forEach(script => {
        const currentSrc = script.getAttribute('src')
        if (currentSrc && !currentSrc.includes('/@fs/')) {
          script.setAttribute('src', currentSrc.replace('/_nuxt/Users/', '/_nuxt/@fs/Users/'))
        }
      })
    })
  }
})
