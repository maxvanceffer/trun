// https://nuxt.com/docs/api/configuration/nuxt-config
export default defineNuxtConfig({
  modules: [
    '@nuxt/ui'
  ],

  devtools: {
    enabled: false
  },

  css: ['~/assets/css/main.css'],

  compatibilityDate: '2026-06-30',

  // Icon collections for tree view
  icon: {
    collections: ['logos']
  },

  // Tauri desktop app — run as SPA to avoid SSR hydration issues
  ssr: false,
  
  // Configure Vite to use proper paths
  vite: {
    clearScreen: false,
    server: {
      hmr: {
        overlay: false
      },
      fs: {
        allow: ['..']
      }
    }
  },
  
  // Output to dist directory
  nitro: {
    output: {
      dir: '.output/public'
    }
  }
})
