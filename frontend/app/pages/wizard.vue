<template>
  <div class="h-dvh w-full flex items-center justify-center bg-[#0a0a0a] text-[#ddd]" style="overflow: hidden;">
    <div class="w-[520px] flex flex-col items-center gap-6 p-8">
      <!-- Logo / Icon -->
      <div class="w-16 h-16 rounded-xl bg-gradient-to-br from-blue-500 to-cyan-400 flex items-center justify-center shadow-lg shadow-blue-500/20">
        <svg xmlns="http://www.w3.org/2000/svg" width="32" height="32" viewBox="0 0 24 24" fill="none" stroke="white" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">
          <path d="M15 22v-4a4.8 4.8 0 0 0-1-3.5c3 0 6-2 6-5.5a7 7 0 1 0-14 0c0 3.5 3 5.5 6 5.5a4.8 4.8 0 0 0-1 3.5v4"/>
          <path d="M12 12v6"/>
          <path d="m15 15-3 3-3-3"/>
        </svg>
      </div>

      <div class="text-center">
        <h1 class="text-2xl font-bold text-white mb-2">Welcome to trun</h1>
        <p class="text-sm text-[#888] max-w-[380px]">
          Select a folder to scan for projects. trun will detect package.json, Cargo.toml, go.mod, and more.
        </p>
      </div>

      <!-- Pick folder button -->
      <UButton
        icon="i-lucide-folder-plus"
        label="Choose Folder"
        size="lg"
        class="w-[280px] font-medium"
        :loading="picking"
        @click="pickFolder"
      />

      <!-- Selected folder path -->
      <div v-if="selectedPath" class="w-full bg-[#111] rounded-lg border border-[#222] px-4 py-3">
        <div class="text-[10px] text-[#555] uppercase tracking-wider mb-1">Selected Folder</div>
        <div class="text-sm text-[#ddd] truncate font-mono">{{ selectedPath }}</div>
      </div>

      <!-- Scan progress -->
      <div v-if="scanning" class="w-full flex items-center gap-3">
        <UProgress
          :model-value="scanProgress"
          :max="100"
          size="sm"
          class="flex-1"
        />
        <span class="text-xs text-[#888] min-w-[3ch] text-right">{{ scanProgress }}%</span>
      </div>

      <!-- Scan complete -->
      <div v-if="scanComplete && !scanning" class="text-center">
        <div class="text-green-400 text-sm font-medium mb-1">Scan complete!</div>
        <div class="text-xs text-[#888]">
          Found {{ scannedCount }} project{{ scannedCount !== 1 ? 's' : '' }}
        </div>
        <UButton
          v-if="scannedCount > 0"
          label="Go to Dashboard"
          icon="i-lucide-arrow-right"
          size="sm"
          class="mt-3"
          @click="goToDashboard"
        />
      </div>

      <!-- Error -->
      <div v-if="error" class="w-full bg-red-500/10 border border-red-500/20 rounded-lg px-4 py-3 text-red-400 text-sm text-center">
        {{ error }}
      </div>

      <!-- Footer -->
      <div class="text-[11px] text-[#444] mt-auto">
        Supports: package.json · Cargo.toml · go.mod · Gemfile · pyproject.toml · & more
      </div>
    </div>
  </div>
</template>

<script setup lang="ts">
import { ref } from 'vue'
import { useTauri } from '~/composables/useTauri'

const { invoke } = useTauri()

// State
const picking = ref(false)
const scanning = ref(false)
const scanProgress = ref(0)
const scanComplete = ref(false)
const scannedCount = ref(0)
const selectedPath = ref('')
const error = ref('')

async function pickFolder() {
  picking.value = true
  error.value = ''
  try {
    const result = await invoke('cmd_pick_folder')
    if (result) {
      selectedPath.value = result
      await startScan(result)
    } else {
      error.value = 'No folder selected.'
    }
  } catch (err: any) {
    error.value = err.message || String(err)
  } finally {
    picking.value = false
  }
}

async function startScan(rootPath: string) {
  scanning.value = true
  scanProgress.value = 0
  scanComplete.value = false
  scannedCount.value = 0

  try {
    // Scan
    const result = await invoke('cmd_scan', { rootPath })
    const projects = Array.isArray(result) ? result : []
    scannedCount.value = projects.length

    scanning.value = false
    scanComplete.value = true

    // Auto-show dashboard after scan
    if (projects.length > 0) {
      goToDashboard()
    } else {
      error.value = 'No projects found. Try another folder.'
      scanning.value = false
    }
  } catch (err: any) {
    error.value = err.message || String(err)
    scanning.value = false
  }
}

async function goToDashboard() {
  try {
    await invoke('cmd_show_dashboard')
  } catch (err: any) {
    // Fallback for browser dev mode (no Tauri)
    window.location.href = window.location.origin + '/'
  }
}
</script>
