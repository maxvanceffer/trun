/**
 * Unit tests for sidebar tree view selection logic.
 * Tests buildTreeItems, onTreeSelect, and selectProject in isolation.
 */
import { describe, it, expect, beforeEach } from 'vitest'

// ─── Types (mirror index.vue) ────────────────────────────────────────────

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

// ─── Logic (mirrors index.vue) ───────────────────────────────────────────

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

// ─── Selection state (mirrors index.vue) ─────────────────────────────────

type ActiveProject = { id: string; name: string; root_path: string } | null
let activeProject: ActiveProject = null
let activeProjectCommands: CommandInfo[] = []

function onTreeSelect(item: TreeItem, _projects: ProjectInfo[]): { projectSelected: boolean } {
  if (!item || !item.id) return { projectSelected: false }
  const project = _projects.find(p => p.id === item.id)
  if (project) {
    activeProject = { id: project.id, name: project.name, root_path: project.root_path }
    activeProjectCommands = project.commands || []
    return { projectSelected: true }
  }
  return { projectSelected: false }
}

// ─── Tests ───────────────────────────────────────────────────────────────

describe('buildTreeItems', () => {
  it('should build tree for a single project', () => {
    const projects: ProjectInfo[] = [{
      id: '/home/user/projects/myapp',
      name: 'myapp', project_path: '/home/user/projects/myapp',
      root_path: '/home/user/projects/myapp', icon: 'lucide:folder',
      manifest: 'package.json', commands: []
    }]
    const tree = buildTreeItems(projects)
    expect(tree).toHaveLength(1)
    expect(tree[0].id).toBe('/home/user/projects/myapp')
    expect(tree[0].projectCount).toBe(1)
  })

  it('should build multi-manifest folder with children', () => {
    const projects: ProjectInfo[] = [
      { id: '/work:package.json', name: 'work', project_path: '/work', root_path: '/work', icon: '', manifest: 'package.json', commands: [] },
      { id: '/work:Cargo.toml', name: 'work-rust', project_path: '/work', root_path: '/work', icon: '', manifest: 'Cargo.toml', commands: [] }
    ]
    const tree = buildTreeItems(projects)
    expect(tree[0].isMultiManifestFolder).toBe(true)
    expect(tree[0].children).toHaveLength(2)
    expect(tree[0].children!.map(c => c.label)).toContain('npm')
    expect(tree[0].children!.map(c => c.label)).toContain('cargo')
  })
})

describe('onTreeSelect', () => {
  beforeEach(() => { activeProject = null; activeProjectCommands = [] })

  it('should select a project when clicking a leaf item', () => {
    const projects: ProjectInfo[] = [{
      id: '/myapp', name: 'myapp', project_path: '/myapp', root_path: '/myapp',
      icon: '', manifest: 'package.json', commands: [{ id: '1', category: 'npm', label: 'dev', command: 'npm', args: [] }]
    }]
    const tree = buildTreeItems(projects)
    const result = onTreeSelect(tree[0], projects)
    expect(result.projectSelected).toBe(true)
    expect(activeProject?.name).toBe('myapp')
  })

  it('should select different projects with different commands', () => {
    const projects: ProjectInfo[] = [
      { id: '/projects/app-a', name: 'App A', project_path: '/projects/app-a', root_path: '/projects', icon: '', manifest: 'package.json', commands: [{ id: 'd', category: 'npm', label: 'dev', command: 'npm', args: [] }] },
      { id: '/projects/app-b', name: 'App B', project_path: '/projects/app-b', root_path: '/projects', icon: '', manifest: 'package.json', commands: [{ id: 'd', category: 'npm', label: 'dev', command: 'npm', args: [] }, { id: 't', category: 'npm', label: 'test', command: 'npm', args: ['test'] }] }
    ]
    const tree = buildTreeItems(projects)
    // Root folder contains both projects as children
    expect(tree).toHaveLength(1)
    expect(tree[0].label).toBe('projects')
    expect(tree[0].children).toHaveLength(2)

    const resultA = onTreeSelect(tree[0].children![0], projects)
    expect(resultA.projectSelected).toBe(true)
    expect(activeProject?.name).toBe('App A')

    const resultB = onTreeSelect(tree[0].children![1], projects)
    expect(resultB.projectSelected).toBe(true)
    expect(activeProject?.name).toBe('App B')
    expect(activeProjectCommands).toHaveLength(2)
  })

  it('should select correct project when clicking manifest child item', () => {
    const projects: ProjectInfo[] = [
      { id: '/work:package.json', name: 'work', project_path: '/work', root_path: '/work', icon: '', manifest: 'package.json', commands: [{ id: 'npm-dev', category: 'npm', label: 'dev', command: 'npm', args: ['run', 'dev'] }] },
      { id: '/work:Cargo.toml', name: 'work-rust', project_path: '/work', root_path: '/work', icon: '', manifest: 'Cargo.toml', commands: [{ id: 'cargo-run', category: 'cargo', label: 'run', command: 'cargo', args: ['run'] }] }
    ]
    const tree = buildTreeItems(projects)
    const npmChild = tree[0].children!.find(c => c.label === 'npm')!
    const cargoChild = tree[0].children!.find(c => c.label === 'cargo')!

    const npmResult = onTreeSelect(npmChild, projects)
    expect(npmResult.projectSelected).toBe(true)
    expect(activeProject?.id).toBe('/work:package.json')

    const cargoResult = onTreeSelect(cargoChild, projects)
    expect(cargoResult.projectSelected).toBe(true)
    expect(activeProject?.id).toBe('/work:Cargo.toml')
  })
})
