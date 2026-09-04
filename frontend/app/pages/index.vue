<template>
  <div class="h-dvh w-full flex bg-[#0a0a0a] text-[#ddd]">
    <!-- Sidebar -->
    <aside class="w-[240px] min-w-[240px] bg-[#111] border-r border-[#222] flex flex-col">
      <!-- Header -->
      <div class="p-3 border-b border-[#222]">
        <h2 class="text-xs font-semibold text-[#888] uppercase tracking-wider">Projects</h2>
      </div>
      <!-- Project tree -->
      <div class="flex-1 overflow-y-auto px-2 pb-2">
        <UTree
          v-if="treeItems.length > 0"
          :items="treeItems"
          :getKey="(item) => item.id"
          color="neutral"
          size="sm"
          class="text-[#ddd]"
          :ui="{
            listWithChildren: '',
            itemWithChildren: 'ps-1',
            link: 'text-[#aaa] hover:text-[#ddd] hover:before:bg-[#1a1a1a] cursor-pointer',
            linkLabel: 'text-[#ddd] truncate',
            linkLeadingIcon: 'shrink-0 relative',
            linkTrailing: 'ms-auto shrink-0 flex items-center gap-1',
            linkTrailingIcon: 'text-[#444] text-[9px]'
          }"
          :onSelect="safeOnTreeSelect"
        >
          <template #item-leading="{ item }">
            <Icon v-if="item.icon" :name="item.icon" class="w-4 h-4 shrink-0" />
          </template>
          <template #item-label="{ item }">
            <span class="truncate">{{ item.label }}</span>
          </template>
          <template #item-trailing="{ item }">
            <div class="flex items-center gap-1">
              <button
                v-if="item.isFolder && !item.isCustomCommand"
                @click.stop="openAddCustomCommand(item)"
                class="w-4 h-4 flex items-center justify-center rounded hover:bg-[#222] text-[#555] hover:text-[#888] transition-colors"
                title="Add custom command"
              >
                <Icon name="i-lucide-plus" class="w-3 h-3" />
              </button>
              <span v-if="item.projectCount && item.projectCount > 0 && !item.isMultiManifestFolder" class="text-[#444] text-[9px]">{{ item.projectCount }}</span>
            </div>
          </template>
        </UTree>
        <div v-else class="p-4 text-center text-[#444] text-xs">No projects found</div>
      </div>
      <!-- Scan button -->
      <div class="p-2 border-t border-[#222]">
        <UButton icon="i-lucide-folder-open" label="Rescan" size="sm" variant="ghost" color="neutral" class="w-full" @click="handleRescan" />
      </div>

    </aside>

    <!-- Custom command modal overlay -->
    <Teleport to="body">
      <div v-if="customCmdModalOpen" class="fixed inset-0 z-[9999] bg-black/60 flex items-center justify-center" @click.self="customCmdModalOpen = false" @keydown.escape="customCmdModalOpen = false">
        <div class="w-[380px] bg-[#161616] border border-[#2a2a2a] rounded-lg shadow-2xl">
          <div class="flex items-center justify-between p-3 border-b border-[#2a2a2a]">
            <h3 class="text-sm font-semibold text-[#ddd]">Add Custom Command</h3>
            <button @click="customCmdModalOpen = false" class="text-[#555] hover:text-[#888]">
              <Icon name="i-lucide-x" class="w-4 h-4" />
            </button>
          </div>

          <div class="p-4 space-y-3">
            <div>
              <label class="text-[11px] text-[#888] mb-1 block">Command name</label>
              <UInput v-model="customCmdForm.name" placeholder="e.g. lint:fix" size="sm" class="w-full" />
            </div>
            <div>
              <label class="text-[11px] text-[#888] mb-1 block">Binary</label>
              <UInput v-model="customCmdForm.binary" placeholder="e.g. npm, yarn, pnpm" size="sm" class="w-full" />
            </div>
            <div>
              <label class="text-[11px] text-[#888] mb-1 block">Args (space-separated)</label>
              <UInput v-model="customCmdForm.args" placeholder="e.g. run lint --fix" size="sm" class="w-full" />
            </div>
            <div>
              <label class="text-[11px] text-[#888] mb-1 block">Working directory</label>
              <UInput v-model="customCmdForm.workingDirectory" size="sm" class="w-full" />
            </div>
            <div>
              <label class="text-[11px] text-[#888] mb-1 block">Environment variables</label>
              <div class="space-y-2">
                <div v-for="(env, idx) in customCmdForm.envs" :key="idx" class="flex gap-2">
                  <UInput v-model="env.key" placeholder="KEY" size="sm" class="flex-1" />
                  <UInput v-model="env.value" placeholder="value" size="sm" class="flex-1" />
                  <button @click="removeEnvVar(idx)" class="text-[#555] hover:text-[#888] shrink-0" :disabled="customCmdForm.envs.length <= 1">
                    <Icon name="i-lucide-x" class="w-4 h-4" />
                  </button>
                </div>
                <UButton label="+ Add variable" size="xs" variant="ghost" color="neutral" @click="addEnvVar" />
              </div>
            </div>
            <div v-if="customCmdForm.name && !customCommands.some(c => c.name === customCmdForm.name)">
              <label class="text-[11px] text-[#444] mb-1 block">Preview icon</label>
              <div class="flex items-center gap-2 text-xs">
                <Icon :name="getCustomCmdIcon(customCmdForm.binary)" class="w-4 h-4" />
                <span class="text-[#888] font-mono">{{ customCmdForm.binary }}</span>
                <span v-if="customCmdForm.args" class="text-[#555]">{{ customCmdForm.args }}</span>
              </div>
            </div>
          </div>

          <div class="flex justify-end gap-2 p-3 border-t border-[#2a2a2a]">
            <UButton label="Cancel" size="sm" variant="ghost" color="neutral" @click="customCmdModalOpen = false" />
            <UButton
              label="Add command"
              size="sm"
              variant="solid"
              color="green"
              :disabled="!customCmdForm.name || !customCmdForm.binary"
              @click="addCustomCommand"
            />
          </div>
        </div>
      </div>
    </Teleport>

    <!-- Main content -->
    <main class="flex-1 flex flex-col min-h-0 overflow-hidden">
      <!-- Header with stats -->
      <header class="flex items-center justify-between px-4 py-2 border-b border-[#222]">
        <div class="flex items-center gap-2">
          <h1 ref="titleEl" class="text-sm font-semibold text-[#ddd]">Dashboard</h1>
          <span class="text-[10px] px-1.5 py-0.5 rounded-full bg-[#22c55e]/20 text-[#22c55e] border border-[#22c55e]/30">● Live</span>
        </div>
        <div class="text-xs text-[#888] flex gap-3">
          <span>CPU: {{ systemStats.cpu.toFixed(1) }}%</span>
          <span>RAM: {{ systemStats.ramUsed }} MB / {{ systemStats.ramTotal }} MB</span>
        </div>
      </header>

      <!-- Commands section (top) -->
      <div v-if="activeProject" class="border-b border-[#222]">
        <div v-for="section in commandSections" :key="section.category" class="mb-2 last:mb-0">
          <!-- Category header -->
          <div class="flex items-center gap-1.5 px-4 pt-3 pb-1.5">
            <Icon :name="getCategoryIcon(section.category)" class="w-4 h-4 opacity-80" />
            <span class="text-[11px] font-semibold text-[#888] uppercase tracking-wider capitalize">{{ getCategoryLabel(section.category) }}</span>
            <span class="text-[10px] text-[#444]">({{ section.commands.length }})</span>
          </div>
          <!-- Command cards -->
          <div class="grid grid-cols-1 sm:grid-cols-2 lg:grid-cols-3 gap-2 px-4 pb-3">
            <UCard
              v-for="cmd in section.commands"
              :key="cmd.id"
              :ui="{
                root: 'bg-[#1a1a1a] border-[#2a2a2a] hover:border-[#333] transition-all group',
                body: 'flex items-center justify-between gap-3 px-3 py-2.5',
                title: 'text-xs font-medium text-[#ddd]',
              }"
            >
              <div class="flex-1 min-w-0">
                <div class="flex items-center gap-2">
                  <span v-if="isCommandRunning(cmd.id)" class="text-[#f87171] animate-pulse">●</span>
                  <span v-else class="text-[#22c55e]">○</span>
                  <span class="truncate text-xs">{{ cmd.label }}</span>
                </div>
                <div class="text-[10px] text-[#555] mt-1 font-mono truncate">
                  {{ cmd.command }} {{ cmd.args.join(' ') }}
                </div>
                <!-- Running info: PID + Memory -->
                <div v-if="isCommandRunning(cmd.id)" class="flex items-center gap-3 mt-1.5">
                  <span class="text-[10px] text-[#f87171] flex items-center gap-1">
                    <Icon name="i-lucide-terminal" class="w-3 h-3" />
                    PID {{ getProcessPid(cmd.id) }}
                  </span>
                  <span class="text-[10px] text-[#60a5fa] flex items-center gap-1">
                    <Icon name="i-lucide-circle-help" class="w-3 h-3" />
                    {{ getProcessMemory(cmd.id) }} MB
                  </span>
                </div>
              </div>
              <UButton
                :icon="isCommandRunning(cmd.id) ? 'i-lucide-square' : 'i-lucide-play'"
                :label="isCommandRunning(cmd.id) ? 'Stop' : 'Run'"
                :color="isCommandRunning(cmd.id) ? 'red' : 'green'"
                size="xs"
                variant="solid"
                @click="isCommandRunning(cmd.id) ? handleStopCommand(cmd.id) : handleRunCommand(cmd.id, cmd.label, cmd.command, cmd.args)"
              />
            </UCard>
          </div>
        </div>
      </div>

      <!-- Logs panel (bottom) -->
      <div class="flex-1 flex flex-col min-h-0 overflow-hidden">
        <div class="flex items-center justify-between px-4 py-1 border-b border-[#222]">
          <span class="text-[10px] text-[#555] uppercase tracking-wider">Activity</span>
          <button @click="logsContentEl.value && (logsContentEl.value.innerHTML = '')" class="text-[10px] text-[#555] hover:text-[#888] cursor-pointer">Clear</button>
        </div>
        <div ref="logsEl" class="flex-1 overflow-y-auto p-3 font-mono text-xs leading-relaxed">
          <div ref="logsContentEl"></div>
        </div>
      </div>
    </main>
  </div>
</template>

<script setup lang="ts">
import { ref, onMounted, computed, onBeforeUnmount } from 'vue'
import { useTauri } from '~/composables/useTauri'

const { listen, invoke } = useTauri()

// DOM refs
const titleEl = ref<HTMLElement | null>(null)
const logsEl = ref<HTMLElement | null>(null)
const logsContentEl = ref<HTMLElement | null>(null)
const commandsPanelEl = ref<HTMLElement | null>(null)

// Project info from Rust
type CommandInfo = { id: string; category: string; label: string; command: string; args: string[] }
type ProjectInfo = { id: string; name: string; project_path: string; root_path: string; icon: string; manifest: string; commands: CommandInfo[] }
let projects: ProjectInfo[] = []

// Command sections grouped by category
type CommandSection = { category: string; label: string; icon: string; commands: CommandInfo[] }
const commandSections = ref<CommandSection[]>([])

// Reactive tree state
type TreeItem = {
  id: string
  label: string
  icon?: string
  children?: TreeItem[]
  defaultExpanded?: boolean
  projectCount?: number
  // Multi-manifest folder markers
  isMultiManifestFolder?: boolean
  isFolder?: boolean
  manifestTypes?: string[]
  // Custom command tree item
  isCustomCommand?: boolean
}

// Custom command state
type CustomCommand = { id: string; name: string; binary: string; args: string[]; projectPath: string; workingDirectory: string; envs: { key: string; value: string }[] }
function loadCustomCommands(): CustomCommand[] {
  try {
    const raw = localStorage.getItem('trun_custom_commands')
    return raw ? JSON.parse(raw) : []
  } catch {
    return []
  }
}

function saveCustomCommands(cmds: CustomCommand[]) {
  localStorage.setItem('trun_custom_commands', JSON.stringify(cmds))
}

const customCommands = ref<CustomCommand[]>(loadCustomCommands())
const customCmdModalOpen = ref(false)
const customCmdSelectedFolder = ref<TreeItem | null>(null)
const customCmdForm = ref({ name: '', binary: '', args: '', workingDirectory: '', envs: [{ key: '', value: '' }] })

const treeItems = ref<TreeItem[]>([])
const selectedIds = ref<string[]>([])
const expandedIds = ref<string[]>([])

// Auto-expand directories that have descendant projects
const defaultExpandedIds = computed(() => {
  return expandedIds.value
})

// Active project state
let activeProject: { id: string; name: string; root_path: string } | null = null
let activeProjectCommands: CommandInfo[] = []

// Running processes state: key = `${project.id}:${cmd.id}`
type RunningProcess = {
  pid: number
  memoryMb: number
  status: string
}
const runningProcesses = ref<Map<string, RunningProcess>>(new Map())

// Generate unique key for a command
function cmdKey(cmd: CommandInfo): string {
  return `${activeProject?.id}:${cmd.id}`
}

// Check if a command is running
function isCommandRunning(cmdId: string): boolean {
  return runningProcesses.value.has(cmdId)
}

// Get PID for a running command
function getProcessPid(cmdId: string): number {
  return runningProcesses.value.get(cmdId)?.pid || 0
}

// Get memory usage for a running command
function getProcessMemory(cmdId: string): number {
  return runningProcesses.value.get(cmdId)?.memoryMb || 0
}

// Stop a running command
async function handleStopCommand(cmdId: string) {
  if (!activeProject) return
  addLog('info', 'stop', `Stopping process ${cmdId}...`)
  try {
    await invoke('cmd_stop', { projectId: activeProject.id })
    runningProcesses.value.delete(cmdId)
    addLog('info', 'stop', `Stopped: ${cmdId}`)
  } catch (err: any) {
    addLog('error', 'stop', err.message || String(err))
  }
}

// Category icon mapping (Nuxt UI logos collection)
const categoryIcons: Record<string, string> = {
  npm: 'i-logos-npm-icon',
  cargo: 'i-logos-cargo',
  go: 'i-logos-go',
  ruby: 'i-logos-ruby',
  python: 'i-logos-python',
  elixir: 'i-logos-elixir',
  php: 'i-logos-php',
  composer: 'i-logos-composer',
  custom: 'i-lucide-terminal-square',
  other: 'i-lucide-folder',
}

// System stats
const systemStats = ref<{ cpu: number; ramUsed: number; ramTotal: number }>({ cpu: 0, ramUsed: 0, ramTotal: 0 })
let statsTimer: ReturnType<typeof setInterval> | null = null

// Logs (reactive, shown in panel)
const logs = ref<Array<{level: string; target: string; message: string; timestamp: string}>>([])

// Manifest file → icon mapping (Nuxt UI logos collection)
function manifestIcon(manifest: string): string {
  switch (manifest) {
    case 'package.json': return 'i-logos-npm-icon'
    case 'composer.json': return 'i-logos-composer'
    case 'Cargo.toml': return 'i-logos-cargo-cli'
    case 'go.mod': return 'i-logos-go'
    case 'Gemfile': return 'i-logos-ruby'
    case 'pyproject.toml': return 'i-logos-python'
    case 'mix.exs': return 'i-logos-elixir'
    case 'pom.xml': return 'i-logos-java'
    case 'build.gradle': return 'i-logos-gradle'
    default: return 'i-lucide-file-code'
  }
}

// Manifest file → label mapping
function manifestLabel(manifest: string): string {
  switch (manifest) {
    case 'package.json': return 'npm'
    case 'composer.json': return 'composer'
    case 'Cargo.toml': return 'cargo'
    case 'go.mod': return 'go'
    case 'Gemfile': return 'ruby'
    case 'pyproject.toml': return 'python'
    case 'mix.exs': return 'elixir'
    case 'pom.xml': return 'maven'
    case 'build.gradle': return 'gradle'
    default: return manifest
  }
}

// Manifest file → category for command grouping
function manifestCategory(manifest: string): string {
  switch (manifest) {
    case 'package.json': return 'npm'
    case 'composer.json': return 'composer'
    case 'Cargo.toml': return 'cargo'
    case 'go.mod': return 'go'
    case 'Gemfile': return 'ruby'
    case 'pyproject.toml': return 'python'
    default: return 'other'
  }
}

// Strip manifest suffix from id to get directory path
// Multi-manifest folders have ids like "/path:package.json" → strip ":package.json"
// Single-manifest folders have ids like "/path" → return as-is
function dirPathFromId(id: string): string {
  const colonIdx = id.indexOf(':')
  if (colonIdx === -1) return id
  return id.substring(0, colonIdx)
}

// Check if an id has manifest suffix
function hasManifestSuffix(id: string): boolean {
  return id.includes(':')
}

// Get icon for a custom command based on binary
function getCustomCmdIcon(binary: string): string {
  const b = binary.toLowerCase()
  if (b.includes('npm') || b.includes('npx')) return 'i-logos-npm-icon'
  if (b.includes('cargo')) return 'i-logos-cargo-cli'
  if (b.includes('composer')) return 'i-logos-composer'
  if (b.includes('go')) return 'i-logos-go'
  if (b.includes('ruby') || b.includes('bundle')) return 'i-logos-ruby'
  if (b.includes('python') || b.includes('pip') || b.includes('poetry')) return 'i-logos-python'
  return 'i-lucide-terminal'
}

// Open custom command modal
function openAddCustomCommand(item: TreeItem) {
  customCmdSelectedFolder.value = item
  // For multi-manifest folders the id IS the directory path (no suffix). For suffixed IDs, strip the suffix.
  const wd = hasManifestSuffix(item.id) ? dirPathFromId(item.id) : item.id
  customCmdForm.value = { name: '', binary: '', args: '', workingDirectory: wd, envs: [{ key: '', value: '' }] }
  customCmdModalOpen.value = true
}

function addEnvVar() {
  customCmdForm.value.envs.push({ key: '', value: '' })
}

function removeEnvVar(idx: number) {
  if (customCmdForm.value.envs.length > 1) {
    customCmdForm.value.envs.splice(idx, 1)
  }
}

// Add custom command
function addCustomCommand() {
  if (!customCmdForm.value.name || !customCmdForm.value.binary) return
  
  // Check for duplicate command name
  if (customCommands.value.some(c => c.name === customCmdForm.value.name)) {
    addLog('error', 'custom', `Command '${customCmdForm.value.name}' already exists`)
    customCmdModalOpen.value = false
    return
  }
  
  // Get the project path from the selected folder
  let projectPath = ''
  if (customCmdSelectedFolder.value) {
    // Find any project under this folder
    const found = projects.find(p => p.id.startsWith(customCmdSelectedFolder.value!.id) || customCmdSelectedFolder.value!.id.includes(p.id))
    if (found) {
      projectPath = found.id
    } else {
      // Fallback: use the folder's own ID as project path
      projectPath = customCmdSelectedFolder.value.id
    }
  }
  
  if (!projectPath) {
    addLog('error', 'custom', 'Could not determine project path')
    customCmdModalOpen.value = false
    return
  }
  
  const args = customCmdForm.value.args.split(' ').filter(a => a.trim())
  const workingDir = customCmdForm.value.workingDirectory || projectPath
  const envs = customCmdForm.value.envs.filter(e => e.key.trim())
  const category = manifestCategory(customCmdSelectedFolder.value?.manifestTypes?.[0] || 'package.json')
  
  const cmd: CustomCommand = {
    id: `custom:${Date.now()}`,
    name: customCmdForm.value.name,
    binary: customCmdForm.value.binary,
    args,
    projectPath,
    workingDirectory: workingDir,
    envs
  }
  customCommands.value.push(cmd)
  saveCustomCommands(customCommands.value)
  // Rebuild tree so the new custom command appears in the sidebar
  treeItems.value = buildTreeItems(projects)
  
  addLog('info', 'custom', `Added custom command: ${cmd.name} → ${cmd.binary} ${args.join(' ')}`)
  customCmdModalOpen.value = false
  customCmdForm.value = { name: '', binary: '', args: '', workingDirectory: '', envs: [{ key: '', value: '' }] }
}

// Run custom command
async function runCustomCommand(cmd: CustomCommand) {
  addLog('info', 'custom', `Running custom: ${cmd.name}...`)
  try {
    const envVars: [string, string][] = cmd.envs
      .filter(e => e.key.trim())
      .map(e => [e.key.trim(), e.value])

    await invoke('cmd_run', {
      projectId: cmd.projectPath,
      command: cmd.binary,
      args: cmd.args,
      workingDir: cmd.workingDirectory,
      envVars
    })
    addLog('info', 'custom', `Started: ${cmd.name}`)
  } catch (err: any) {
    addLog('error', 'custom', err.message || String(err))
  }
}

// Build tree items from project list
function buildTreeItems(projs: ProjectInfo[]): TreeItem[] {
  if (projs.length === 0) return []

  const rootPath = projs[0].root_path || ''

  // Group projects by directory path
  // Multi-manifest dirs have ids like "/path/to/dir:package.json"
  // Single-manifest dirs have ids like "/path/to/dir"
  const dirProjects = new Map<string, ProjectInfo[]>()
  projs.forEach(p => {
    const dirPath = dirPathFromId(p.id)
    const list = dirProjects.get(dirPath) || []
    list.push(p)
    dirProjects.set(dirPath, list)
  })

  // Collect all paths: root + dir paths + intermediate dirs
  const allPaths = new Set<string>()
  allPaths.add(rootPath)

  dirProjects.forEach((_, dirPath) => {
    allPaths.add(dirPath)
    let dir = dirPath
    while (dir !== rootPath && dir.startsWith(rootPath)) {
      dir = dir.substring(0, dir.lastIndexOf('/'))
      if (dir && dir !== rootPath) {
        allPaths.add(dir)
      }
    }
  })

  // Build node map
  const nodes = new Map<string, TreeItem & { projectCount: number }>()

  allPaths.forEach(path => {
    const name = path.split('/').filter(Boolean).pop() || path
    nodes.set(path, {
      id: path,
      label: name,
      children: [],
      projectCount: 0
    })
  })

  // Link children to parents
  nodes.forEach((node, path) => {
    if (path === rootPath) return
    const parentPath = path.substring(0, path.lastIndexOf('/'))
    const parent = nodes.get(parentPath)
    if (parent) {
      parent.children!.push(node)
    }
  })

  // Sort: alphabetically
  function sortItems(items: TreeItem[]) {
    items.sort((a, b) => a.label.localeCompare(b.label))
    items.forEach(i => i.children && sortItems(i.children))
  }
  sortItems(nodes.get(rootPath)!.children!)

  // Count descendant projects (including multi-manifest children)
  function countProjects(node: TreeItem & { projectCount: number }): number {
    const path = node.id
    const projsAtDir = dirProjects.get(path)
    if (projsAtDir) return projsAtDir.length
    return node.children!.reduce((sum, child) => sum + countProjects(child as any), 0)
  }
  function countAndAttach(node: TreeItem & { projectCount: number }) {
    node.projectCount = countProjects(node)
    node.children!.forEach(countAndAttach)
  }
  countAndAttach(nodes.get(rootPath)! as any)

  // Collect folder IDs (non-project nodes) for auto-expansion
  expandedIds.value = Array.from(allPaths)
    .filter(path => path !== rootPath)

  // Convert to TreeItem[]
  function toTreeItems(node: TreeItem & { projectCount: number }): TreeItem[] {
    const path = node.id
    const projsAtDir = dirProjects.get(path)
    
    if (!projsAtDir || projsAtDir.length === 0) {
      // Pure folder node
      const item: TreeItem = {
        id: path,
        label: node.label,
        icon: 'i-lucide-folder',
        defaultExpanded: node.projectCount > 0,
        projectCount: node.projectCount,
        isFolder: true,
        children: []
      }
      if (node.children!.length > 0) {
        // Recursively process descendant nodes (projects may be deeper)
        item.children = node.children!.flatMap(toTreeItems)
      }
      // Add custom commands for this folder
      const folderCmds = customCommands.value.filter(c => c.projectPath === path || dirPathFromId(c.projectPath) === path)
      folderCmds.forEach(cmd => {
        item.children!.push({
          id: cmd.id,
          label: cmd.name,
          icon: getCustomCmdIcon(cmd.binary),
          defaultExpanded: false,
          projectCount: 0,
          isCustomCommand: true
        })
      })
      return [item]
    }
    
    // Has projects at this directory
    if (projsAtDir.length === 1 && !hasManifestSuffix(projsAtDir[0].id)) {
      const proj = projsAtDir[0]
      const item: TreeItem = {
        id: proj.id,
        label: proj.name,
        icon: manifestIcon(proj.manifest),
        defaultExpanded: false,
        projectCount: 1,
        isFolder: false
      }
      // Add custom commands for this project
      const projCmds = customCommands.value.filter(c => c.projectPath === proj.id || c.projectPath === proj.project_path)
      projCmds.forEach(cmd => {
        if (!item.children!) item.children = []
        item.children.push({
          id: cmd.id,
          label: cmd.name,
          icon: getCustomCmdIcon(cmd.binary),
          defaultExpanded: false,
          projectCount: 0,
          isCustomCommand: true
        })
      })
      // Auto-expand projects with custom commands
      if (projCmds.length > 0) {
        item.defaultExpanded = true
      }
      return [item]
    }
    
    // Multi-manifest or single with suffix — folder with children
    const dirName = projsAtDir[0].name || path.split('/').filter(Boolean).pop() || ''
    const manifestTypes = [...new Set(projsAtDir.map(p => p.manifest))]
    
    // Determine folder icon: if mixed manifests, show a folder
    const isMixed = manifestTypes.length > 1 || manifestTypes.every(m => m !== 'package.json')
    const folderIcon = isMixed ? 'i-lucide-folder-git-2' : 'i-lucide-folder'
    
    const item: TreeItem = {
      id: path,
      label: dirName,
      icon: folderIcon,
      defaultExpanded: true,
      projectCount: projsAtDir.length,
      isMultiManifestFolder: true,
      isFolder: true,
      manifestTypes,
      children: []
    }
    
    // Create children for each project at this directory
    projsAtDir.forEach(proj => {
      item.children!.push({
        id: proj.id,
        label: proj.name,
        icon: manifestIcon(proj.manifest),
        defaultExpanded: false,
        projectCount: 1
      })
    })
    
    // Add custom commands for this folder
    console.log('[DEBUG BTI] folder path:', path, 'customCommands:', JSON.stringify(customCommands.value.map(c => ({name: c.name, projectPath: c.projectPath}))))
    const folderCmds = customCommands.value.filter(c => c.projectPath === path || dirPathFromId(c.projectPath) === path)
    console.log('[DEBUG BTI] folder', path, 'matched', folderCmds.length, 'commands')
    folderCmds.forEach(cmd => {
      item.children!.push({
        id: cmd.id,
        label: cmd.name,
        icon: getCustomCmdIcon(cmd.binary),
        defaultExpanded: false,
        projectCount: 0,
        isCustomCommand: true
      })
    })
    
    return [item]
  }

  const root = nodes.get(rootPath)!
  return toTreeItems(root as any)
}

// Handle tree item selection (Reka UI TreeItem select event)
// UTree passes (event, item) - capture the full item as second arg
function onTreeSelect(_event: Event | undefined | null, item: TreeItem | undefined | null) {
  if (!item || !item.id) return

  // Handle custom command items
  if (item.isCustomCommand) {
    const cmd = customCommands.value.find(c => c.id === item.id)
    if (cmd) {
      runCustomCommand(cmd)
    }
    return
  }

  // Find the project by id
  const project = projects.find(p => p.id === item.id)
  if (project) {
    selectProject(project)
  }
}

// Wrapper that handles both function and array-of-functions (Vue 3.5+ event batching)
function safeOnTreeSelect(_event: Event | undefined | null, item: TreeItem | undefined | null) {
  onTreeSelect(_event, item)
}

function renderProjectDetails() {
  if (!titleEl.value) return
  titleEl.value.textContent = activeProject?.name || 'Dashboard'
}

function getCategoryIcon(category: string): string {
  return categoryIcons[category] || 'i-lucide-folder'
}

function getCategoryLabel(category: string): string {
  const labels: Record<string, string> = {
    npm: 'npm',
    cargo: 'Cargo',
    go: 'Go',
    ruby: 'Ruby',
    python: 'Python',
    elixir: 'Elixir',
    php: 'PHP',
    composer: 'Composer',
    custom: 'Custom',
    other: 'Other',
  }
  return labels[category] || category
}

// Group commands by category
function groupCommandsByCategory(commands: CommandInfo[]): CommandSection[] {
  const groups = new Map<string, CommandInfo[]>()
  commands.forEach(cmd => {
    const list = groups.get(cmd.category) || []
    list.push(cmd)
    groups.set(cmd.category, list)
  })
  return Array.from(groups.entries()).map(([category, cmds]) => ({
    category,
    label: getCategoryLabel(category),
    icon: getCategoryIcon(category),
    commands: cmds.sort((a, b) => a.label.localeCompare(b.label)),
  }))
}

function addLog(level: string, target: string, message: string) {
  const ts = new Date().toLocaleTimeString('en-US', { hour12: false })
  logs.value.push({ level, target, message, timestamp: ts })
  // Keep max 200 log lines
  if (logs.value.length > 200) logs.value.shift()

  if (!logsContentEl.value) return
  const line = document.createElement('div')
  line.style.color = level === 'error' ? '#e24949' : level === 'warn' ? '#f59e0b' : '#aaa'
  line.textContent = `[${ts}] ${target}: ${message}`
  logsContentEl.value.appendChild(line)

  // Auto-scroll
  if (logsEl.value) {
    logsEl.value.scrollTop = logsEl.value.scrollHeight
  }
}

// ─── System Stats Polling ───────────────────────────────────────────

async function updateSystemStats() {
  try {
    const stats = await invoke<{ cpu_percent: number; ram_used_mb: number; ram_total_mb: number }>('cmd_get_system_stats')
    if (stats && typeof stats.cpu_percent === 'number') {
      systemStats.value = {
        cpu: stats.cpu_percent,
        ramUsed: stats.ram_used_mb,
        ramTotal: stats.ram_total_mb,
      }
    }
  } catch {
    // Silent fail in browser mode
  }
}

async function tailProjectLogs() {
  if (!activeProject) return
  try {
    const logLines = await invoke<{ file: string; line: string; timestamp: string }[]>('cmd_tail_logs', {
      file: `${activeProject.name}.log`,
      lines: 50,
    })
    if (logLines.length > 0) {
      addLog('info', 'log', `Loaded ${logLines.length} lines from ${activeProject.name}.log`)
      logLines.forEach(l => addLog('debug', l.file, l.line))
    }
  } catch {
    // Silent fail
  }
}

// ─── Actions ─────────────────────────────────────────────────────────

async function handleRescan() {
  activeProject = null
  activeProjectCommands = []
  projects = []
  treeItems.value = []
  selectedIds.value = []
  localStorage.removeItem('trun:projects')
  renderProjectDetails()
  addLog('info', 'system', 'Opening wizard for new folder selection...')

  try {
    await invoke('cmd_show_wizard')
  } catch (err: any) {
    addLog('error', 'system', `Failed to show wizard: ${err.message}`)
  }
}

async function selectProject(project: typeof projects[0]) {
  activeProject = { id: project.id, name: project.name, root_path: project.root_path }
  activeProjectCommands = project.commands || []
  
  // Add custom commands for this project path
  const projectCustomCmds = customCommands.value.filter(c => c.projectPath === project.project_path || c.projectPath === project.id)
  if (projectCustomCmds.length > 0) {
    const customCmdInfos: CommandInfo[] = projectCustomCmds.map((c, i) => ({
      id: c.id,
      category: 'custom',
      label: c.name,
      command: c.binary,
      args: c.args
    }))
    activeProjectCommands = [...activeProjectCommands, ...customCmdInfos]
  }
  
  commandSections.value = groupCommandsByCategory(activeProjectCommands)
  treeItems.value = buildTreeItems(projects)
  selectedIds.value = [project.id]
  runningProcesses.value.clear()
  renderProjectDetails()
  addLog('info', 'project', `Selected: ${project.name}`)
  // Tail any existing logs for this project
  tailProjectLogs()
}

async function handleRunCommand(commandId: string, label: string, command: string, args: string[]) {
  if (!activeProject) return
  addLog('info', 'run', `Running: ${label}...`)
  try {
    await invoke('cmd_run', { 
      projectId: activeProject.id, 
      command, 
      args 
    })
    addLog('info', 'run', `Started: ${label}`)
    // Initialize process tracking (will be updated by process:status event)
    const key = commandId
    if (!runningProcesses.value.has(key)) {
      runningProcesses.value.set(key, {
        pid: 0,
        memoryMb: 0,
        status: 'running',
      })
    }
  } catch (err: any) {
    addLog('error', 'run', err.message || String(err))
  }
}

// ─── Event Listeners ─────────────────────────────────────────────────
if (import.meta.client) {
  window.addEventListener('trun:go-dashboard', () => {
    addLog('info', 'system', 'Navigating to dashboard...')
    window.dispatchEvent(new CustomEvent('trun:show-dashboard'))
  })

  window.addEventListener('trun:scan-complete', (event: Event) => {
    const projs = (event as CustomEvent).detail || []
    if (projs.length > 0) {
      addLog('info', 'scan', `Wizard scan complete: ${projs.length} projects`)
    }
  })
}

async function setupListeners() {
  listen('scan:progress', (event: any) => {
    const { path, found } = event.payload || {}
    addLog('debug', 'scan', `Scanning: ${path} (${found} found)`)
  })

  listen('scan:complete', (event: any) => {
    const projectsList = event.payload?.projects || []
    projects = projectsList
    localStorage.setItem('trun:projects', JSON.stringify(projects))
    treeItems.value = buildTreeItems(projects)
    addLog('info', 'scan', `Scan complete: ${projects.length} projects found`)
    if (projects.length > 0) {
      selectProject(projects[0])
    }
  })

  listen('log', (event: any) => {
    const { level, target, message } = event.payload || {}
    addLog(level, target, message)
  })

  listen('command:status', (event: any) => {
    const { project_id, status } = event.payload || {}
    const statusText = `Project ${project_id} → ${status}`
    addLog(status === 'running' ? 'info' : status === 'failed' ? 'error' : 'debug', 'command', statusText)
  })

  listen('process:status', (event: any) => {
    const { project_id, label, pid, status, memory_mb, command } = event.payload || {}
    // Update running processes state
    const key = `${project_id}:${command}`
    if (status === 'running' || status === 'stopped' || status === 'failed') {
      if (status === 'running') {
        runningProcesses.value.set(key, {
          pid,
          memoryMb: memory_mb,
          status,
        })
      } else {
        runningProcesses.value.delete(key)
      }
    }
  })
}

// ─── Lifecycle ───────────────────────────────────────────────────────

onMounted(() => {
  setupListeners()
  renderProjectDetails()

  // Poll system stats every 2s
  updateSystemStats()
  statsTimer = setInterval(updateSystemStats, 2000)

  try {
    const saved = localStorage.getItem('trun:projects')
    if (saved) {
      projects = JSON.parse(saved)
      treeItems.value = buildTreeItems(projects)
      addLog('info', 'system', `Loaded ${projects.length} projects from previous session`)
      // Projects exist — show dashboard, hide wizard
      try {
        invoke('cmd_show_dashboard')
      } catch {
        // Ignore if command not available (e.g. running outside Tauri)
      }
    } else {
      addLog('info', 'system', 'Welcome! Open wizard (or scan from tray) to find projects.')
    }
  } catch {
    addLog('info', 'system', 'No previous projects found.')
  }

  // Cleanup on unmount
  // @ts-expect-error Vue lifecycle
  onBeforeUnmount(() => {
    if (statsTimer) clearInterval(statsTimer)
  })
})</script>
