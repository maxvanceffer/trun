<template>
  <div class="p-4 bg-black text-white min-h-screen">
    <h1 class="text-xl font-bold mb-4">Tree Selection Test</h1>
    <p class="text-[#888] mb-4">Click items in the tree and check the content area below.</p>

    <div class="grid grid-cols-3 gap-4">
      <!-- Tree sidebar -->
      <div class="col-span-1 border border-[#333] rounded-lg p-3">
        <h2 class="text-xs font-semibold text-[#888] uppercase tracking-wider mb-2">Tree</h2>
        <UTree
          v-if="treeItems.length > 0"
          :items="treeItems"
          :getKey="(item) => item.id"
          color="neutral"
          size="sm"
          class="text-white"
          :ui="{
            listWithChildren: '',
            itemWithChildren: 'ps-1',
            link: 'text-[#aaa] hover:text-white hover:before:bg-[#1a1a1a] cursor-pointer',
            linkLabel: 'text-white truncate',
            linkLeadingIcon: 'shrink-0',
            linkTrailing: 'ms-auto shrink-0 flex items-center gap-1',
            linkTrailingIcon: 'text-[#444] text-[9px]'
          }"
          :onSelect="(event, item) => {
            console.log('UTree onSelect callback:', item?.label, item?.id)
            onTreeSelect(event, item)
          }"
        >
          <template #item-leading="{ item }">
            <Icon v-if="item.icon" :name="item.icon" class="w-4 h-4 shrink-0" />
          </template>
          <template #item-label="{ item }">
            <span class="truncate">{{ item.label }}</span>
          </template>
          <template #item-trailing="{ item }">
            <span v-if="item.projectCount" class="text-[#444] text-[9px]">{{ item.projectCount }}</span>
            <Icon v-else-if="item.children?.length" :name="isExpanded(item.id) ? 'i-lucide-chevron-down' : 'i-lucide-chevron-right'" class="w-4 h-4 text-[#444]" />
          </template>
        </UTree>
        <p v-else class="text-[#555] text-sm">No items</p>
      </div>

      <!-- Content area -->
      <div class="col-span-2 border border-[#333] rounded-lg p-3">
        <h2 class="text-xs font-semibold text-[#888] uppercase tracking-wider mb-2">Selected</h2>
        <div v-if="selectedItem" class="bg-[#111] rounded p-3 border border-[#222]">
          <p class="font-bold text-lg">{{ selectedItem.label }}</p>
          <p class="text-[#888] text-sm">ID: {{ selectedItem.id }}</p>
          <p v-if="selectedItem.projectCount" class="text-[#666] text-sm mt-1">Projects: {{ selectedItem.projectCount }}</p>
          <p v-if="selectedItem.isMultiManifestFolder" class="text-[#666] text-sm mt-1">Type: Multi-manifest folder</p>
          <p v-if="selectedItem.manifestTypes" class="text-[#666] text-sm mt-1">Manifests: {{ selectedItem.manifestTypes.join(', ') }}</p>
          <div v-if="selectedProject" class="mt-3 pt-3 border-t border-[#222]">
            <p class="text-green-400 font-semibold">✓ Project loaded: {{ selectedProject.name }}</p>
            <p class="text-[#666] text-sm mt-1">Commands: {{ selectedProject.commands?.length || 0 }}</p>
            <ul v-if="selectedProject.commands?.length" class="mt-2 space-y-1">
              <li v-for="cmd in selectedProject.commands" :key="cmd.id" class="text-[#444] text-xs font-mono">
                {{ cmd.command }} {{ cmd.args.join(' ') }}
              </li>
            </ul>
          </div>
        </div>
        <p v-else class="text-[#555] text-sm">Click an item in the tree to select it</p>
      </div>

      <!-- Console output -->
      <div class="col-span-3 border border-[#333] rounded-lg p-3">
        <h2 class="text-xs font-semibold text-[#888] uppercase tracking-wider mb-2">Console (last 20)</h2>
        <div ref="consoleEl" class="bg-[#0a0a0a] rounded p-2 font-mono text-[10px] text-[#666] overflow-auto max-h-40">
          <div v-for="(log, idx) in consoleLogs" :key="idx">{{ log }}</div>
        </div>
      </div>
    </div>
  </div>
</template>

<script setup lang="ts">
import { ref, onMounted } from 'vue'

interface ProjectInfo {
  id: string
  name: string
  project_path: string
  root_path: string
  icon: string
  manifest: string
  commands: CommandInfo[]
}

interface CommandInfo {
  id: string
  category: string
  label: string
  command: string
  args: string[]
}

interface TreeItem {
  id: string
  label: string
  icon?: string
  children?: TreeItem[]
  defaultExpanded?: boolean
  projectCount?: number
  isMultiManifestFolder?: boolean
  manifestTypes?: string[]
}

// ─── State ───────────────────────────────────────────────────────────────

const treeItems = ref<TreeItem[]>([])
const selectedItem = ref<TreeItem | null>(null)
const selectedProject = ref<ProjectInfo | null>(null)
const consoleLogs = ref<string[]>([])
const consoleEl = ref<HTMLElement | null>(null)

// ─── Projects (mock data) ──────────────────────────────────────────────

const projects = ref<ProjectInfo[]>([
  {
    id: '/home/user/projects/myapp',
    name: 'myapp',
    project_path: '/home/user/projects/myapp',
    root_path: '/home/user/projects',
    icon: 'lucide:folder',
    manifest: 'package.json',
    commands: [
      { id: '1', category: 'npm', label: 'dev', command: 'npm', args: ['run', 'dev'] },
      { id: '2', category: 'npm', label: 'build', command: 'npm', args: ['run', 'build'] }
    ]
  },
  {
    id: '/home/user/projects/work:package.json',
    name: 'work',
    project_path: '/home/user/projects/work',
    root_path: '/home/user/projects/work',
    icon: 'lucide:folder',
    manifest: 'package.json',
    commands: [
      { id: '3', category: 'npm', label: 'dev', command: 'npm', args: ['run', 'dev'] }
    ]
  },
  {
    id: '/home/user/projects/work:Cargo.toml',
    name: 'work-rust',
    project_path: '/home/user/projects/work',
    root_path: '/home/user/projects/work',
    icon: 'lucide:folder',
    manifest: 'Cargo.toml',
    commands: [
      { id: '4', category: 'cargo', label: 'run', command: 'cargo', args: ['run'] }
    ]
  },
  {
    id: '/home/user/projects/api',
    name: 'api',
    project_path: '/home/user/projects/api',
    root_path: '/home/user/projects',
    icon: 'lucide:folder',
    manifest: 'go.mod',
    commands: [
      { id: '5', category: 'go', label: 'run', command: 'go', args: ['run', '.'] }
    ]
  }
])

// ─── Tree building ───────────────────────────────────────────────────────

function dirPathFromId(id: string): string {
  const colonIdx = id.indexOf(':')
  return colonIdx === -1 ? id : id.substring(0, colonIdx)
}

function manifestIcon(manifest: string): string {
  const map: Record<string, string> = {
    'package.json': 'i-logos-npm-icon',
    'Cargo.toml': 'i-logos-cargo-cli',
    'go.mod': 'i-logos-go',
    'Gemfile': 'i-logos-ruby',
    'pyproject.toml': 'i-logos-python',
  }
  return map[manifest] || 'i-lucide-file-code'
}

function manifestLabel(manifest: string): string {
  const map: Record<string, string> = {
    'package.json': 'npm',
    'Cargo.toml': 'cargo',
    'go.mod': 'go',
    'Gemfile': 'ruby',
    'pyproject.toml': 'python',
  }
  return map[manifest] || manifest
}

function buildTreeItems(projs: ProjectInfo[]): TreeItem[] {
  if (projs.length === 0) return []
  const rootPath = projs[0].root_path || ''
  const dirProjects = new Map<string, ProjectInfo[]>()
  projs.forEach(p => {
    const dirPath = dirPathFromId(p.id)
    const list = dirProjects.get(dirPath) || []
    list.push(p)
    dirProjects.set(dirPath, list)
  })
  const allPaths = new Set<string>([rootPath])
  dirProjects.forEach((_, dirPath) => {
    allPaths.add(dirPath)
    let dir = dirPath
    while (dir !== rootPath && dir.startsWith(rootPath)) {
      dir = dir.substring(0, dir.lastIndexOf('/'))
      if (dir && dir !== rootPath) allPaths.add(dir)
    }
  })
  const nodes = new Map<string, TreeItem & { projectCount: number }>()
  allPaths.forEach(path => {
    const name = path.split('/').filter(Boolean).pop() || path
    nodes.set(path, { id: path, label: name, children: [], projectCount: 0 })
  })
  nodes.forEach((node, path) => {
    if (path === rootPath) return
    const parentPath = path.substring(0, path.lastIndexOf('/'))
    const parent = nodes.get(parentPath)
    if (parent) parent.children!.push(node)
  })
  function sortItems(items: TreeItem[]) {
    items.sort((a, b) => a.label.localeCompare(b.label))
    items.forEach(i => i.children && sortItems(i.children))
  }
  sortItems(nodes.get(rootPath)!.children!)
  function countProjects(node: TreeItem & { projectCount: number }): number {
    const projsAtDir = dirProjects.get(node.id)
    return projsAtDir ? projsAtDir.length : node.children!.reduce((s, c) => s + countProjects(c), 0)
  }
  function countAndAttach(node: TreeItem & { projectCount: number }) {
    node.projectCount = countProjects(node)
    node.children!.forEach(countAndAttach)
  }
  countAndAttach(nodes.get(rootPath)! as TreeItem & { projectCount: number })

  function toTreeItems(node: TreeItem & { projectCount: number }): TreeItem[] {
    const projsAtDir = dirProjects.get(node.id)
    if (!projsAtDir || projsAtDir.length === 0) {
      const item: TreeItem = { id: node.id, label: node.label, icon: 'i-lucide-folder', defaultExpanded: node.projectCount > 0, projectCount: node.projectCount, children: [] }
      if (node.children!.length > 0) item.children = node.children!.flatMap(toTreeItems)
      return [item]
    }
    if (projsAtDir.length === 1 && !projsAtDir[0].id.includes(':')) {
      const proj = projsAtDir[0]
      return [{ id: proj.id, label: proj.name, icon: manifestIcon(proj.manifest), defaultExpanded: false, projectCount: 1 }]
    }
    const manifestTypes = [...new Set(projsAtDir.map(p => p.manifest))]
    const isMixed = manifestTypes.length > 1 || manifestTypes.every(m => m !== 'package.json')
    const item: TreeItem = { id: node.id, label: manifestTypes[0], icon: isMixed ? 'i-lucide-folder-git-2' : 'i-lucide-folder', defaultExpanded: true, projectCount: projsAtDir.length, isMultiManifestFolder: true, manifestTypes, children: [] }
    manifestTypes.forEach(manifest => {
      const projForManifest = projsAtDir.find(p => p.manifest === manifest)
      if (projForManifest) item.children!.push({ id: projForManifest.id, label: manifestLabel(manifest), icon: manifestIcon(manifest), defaultExpanded: false, projectCount: 1 })
    })
    return [item]
  }
  return toTreeItems(nodes.get(rootPath)! as TreeItem & { projectCount: number })
}

function isExpanded(id: string): boolean {
  const item = findItem(treeItems.value, id)
  return item?.defaultExpanded || false
}

function findItem(items: TreeItem[], id: string): TreeItem | undefined {
  for (const item of items) {
    if (item.id === id) return item
    if (item.children) {
      const found = findItem(item.children, id)
      if (found) return found
    }
  }
  return undefined
}

// ─── Selection ───────────────────────────────────────────────────────────

function onTreeSelect(event: Event | undefined | null, item: TreeItem | undefined | null) {
  if (!item || !item.id) return

  selectedItem.value = item
  consoleLog(`Selected: ${item.label} (id: ${item.id})`)

  // Find the project by id
  const project = projects.value.find(p => p.id === item.id)
  if (project) {
    selectedProject.value = project
    consoleLog(`✓ Project loaded: ${project.name} with ${project.commands.length} commands`)
  } else {
    selectedProject.value = null
    consoleLog(`⚠ No project found for id: ${item.id} (folder node?)`)
  }
}

function consoleLog(msg: string) {
  const timestamp = new Date().toLocaleTimeString()
  consoleLogs.value = [...consoleLogs.value.slice(-20), `[${timestamp}] ${msg}`]
  // Scroll to bottom
  nextTick(() => {
    if (consoleEl.value) {
      consoleEl.value.scrollTop = consoleEl.value.scrollHeight
    }
  })
}

// ─── Lifecycle ───────────────────────────────────────────────────────────

onMounted(() => {
  treeItems.value = buildTreeItems(projects.value)
  consoleLog(`Tree built with ${treeItems.value.length} root items`)
  consoleLog('Click tree items to test selection')
})
</script>
