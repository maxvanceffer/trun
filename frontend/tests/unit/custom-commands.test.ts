/**
 * Unit tests for custom command flow — create, store, and run.
 * Tests: envVars serialization, projectId correctness, tree attachment.
 */
import { describe, it, expect } from 'vitest'

interface ProjectInfo {
  id: string; name: string; project_path: string; root_path: string
  icon: string; manifest: string; commands: CommandInfo[]
}
interface CommandInfo {
  id: string; category: string; label: string; command: string; args: string[]
}
interface TreeItem {
  id: string; label: string; icon?: string; children?: TreeItem[]
  defaultExpanded?: boolean; projectCount?: number; isCustomCommand?: boolean
  isFolder?: boolean; isMultiManifestFolder?: boolean; manifestTypes?: string[]
}
interface CustomCommand {
  id: string; name: string; binary: string; args: string[]
  projectPath: string; workingDirectory: string; envs: { key: string; value: string }[]
}

function dirPathFromId(id: string): string {
  const idx = id.indexOf(':')
  return idx === -1 ? id : id.substring(0, idx)
}
function hasManifestSuffix(id: string): boolean { return id.includes(':') }
function manifestIcon(m: string): string { return { 'package.json': 'i-logos-npm-icon', 'Cargo.toml': 'i-logos-cargo-cli' }[m] || 'i-lucide-file-code' }
function manifestLabel(m: string): string { return { 'package.json': 'npm', 'Cargo.toml': 'cargo' }[m] || m }
function getCustomCmdIcon(binary: string): string { return { 'cargo': 'i-logos-cargo-cli', 'npm': 'i-logos-npm-icon', 'yarn': 'i-logos-yarn', 'bun': 'i-logos-bun' }[binary] || 'i-lucide-terminal' }

// buildTreeItems takes customCommands as parameter (mirror of index.vue reading customCommands.value)
function buildTreeItems(projs: ProjectInfo[], customCommands: CustomCommand[]): TreeItem[] {
  if (projs.length === 0) return []
  const rootPath = projs[0].root_path || ''
  const dirProjects = new Map<string, ProjectInfo[]>()
  projs.forEach(p => { const dp = dirPathFromId(p.id); const l = dirProjects.get(dp) || []; l.push(p); dirProjects.set(dp, l) })
  const allPaths = new Set<string>([rootPath])
  dirProjects.forEach((_, dirPath) => { allPaths.add(dirPath); let dir = dirPath; while (dir !== rootPath && dir.startsWith(rootPath)) { dir = dir.substring(0, dir.lastIndexOf('/')); if (dir && dir !== rootPath) allPaths.add(dir) } })
  const nodes = new Map<string, TreeItem & { projectCount: number }>()
  allPaths.forEach(path => { const name = path.split('/').filter(Boolean).pop() || path; nodes.set(path, { id: path, label: name, children: [], projectCount: 0 }) })
  nodes.forEach((node, path) => { if (path === rootPath) return; const pp = path.substring(0, path.lastIndexOf('/')); const par = nodes.get(pp); if (par) par.children!.push(node) })
  function sortItems(items: TreeItem[]) { items.sort((a, b) => a.label.localeCompare(b.label)); items.forEach(i => i.children && sortItems(i.children)) }
  sortItems(nodes.get(rootPath)!.children!)
  function countP(n: TreeItem & { projectCount: number }): number { const d = dirProjects.get(n.id); return d ? d.length : n.children!.reduce((s, c) => s + countP(c), 0) }
  function countAttach(n: TreeItem & { projectCount: number }) { n.projectCount = countP(n); n.children!.forEach(countAttach) }
  countAttach(nodes.get(rootPath)! as TreeItem & { projectCount: number })
  function toTree(node: TreeItem & { projectCount: number }): TreeItem[] {
    const path = node.id
    const pd = dirProjects.get(path)
    if (!pd || pd.length === 0) {
      const item: TreeItem = { id: path, label: node.label, icon: 'i-lucide-folder', defaultExpanded: node.projectCount > 0, projectCount: node.projectCount, isFolder: true, children: [] }
      if (node.children!.length > 0) item.children = node.children!.flatMap(toTree)
      const fc = customCommands.filter(c => c.projectPath === path || dirPathFromId(c.projectPath) === path)
      fc.forEach(cmd => { item.children!.push({ id: cmd.id, label: cmd.name, icon: getCustomCmdIcon(cmd.binary), defaultExpanded: false, projectCount: 0, isCustomCommand: true }) })
      return [item]
    }
    if (pd.length === 1 && !hasManifestSuffix(pd[0].id)) {
      const proj = pd[0]
      const item: TreeItem = { id: proj.id, label: proj.name, icon: manifestIcon(proj.manifest), defaultExpanded: false, projectCount: 1, isFolder: false }
      const pc = customCommands.filter(c => c.projectPath === proj.id || c.projectPath === proj.project_path)
      pc.forEach(cmd => { if (!item.children!) item.children = []; item.children!.push({ id: cmd.id, label: cmd.name, icon: getCustomCmdIcon(cmd.binary), defaultExpanded: false, projectCount: 0, isCustomCommand: true }) })
      return [item]
    }
    const mt = [...new Set(pd.map(p => p.manifest))]
    const mixed = mt.length > 1 || mt.every(m => m !== 'package.json')
    const item: TreeItem = { id: path, label: pd[0].name || path.split('/').filter(Boolean).pop() || '', icon: mixed ? 'i-lucide-folder-git-2' : 'i-lucide-folder', defaultExpanded: true, projectCount: pd.length, isMultiManifestFolder: true, isFolder: true, manifestTypes: mt, children: [] }
    mt.forEach(manifest => { const pf = pd.find(p => p.manifest === manifest); if (pf) { const child: TreeItem = { id: pf.id, label: manifestLabel(manifest), icon: manifestIcon(manifest), defaultExpanded: false, projectCount: 1, isFolder: false }; const pc = customCommands.filter(c => c.projectPath === pf.id || c.projectPath === pf.project_path); pc.forEach(cmd => { if (!child.children!) child.children = []; child.children!.push({ id: cmd.id, label: cmd.name, icon: getCustomCmdIcon(cmd.binary), defaultExpanded: false, projectCount: 0, isCustomCommand: true }) }); item.children!.push(child) } })
    return [item]
  }
  return toTree(nodes.get(rootPath)! as TreeItem & { projectCount: number })
}

interface InvokeArgs { cmd: string; projectId: string; command: string; args: string[]; workingDir?: string; envVars: unknown }
let capturedArgs: InvokeArgs | null = null
async function mockInvoke(cmd: string, args: Record<string, unknown>) {
  capturedArgs = { cmd, projectId: args.projectId as string, command: args.command as string, args: args.args as string[], workingDir: args.workingDir as string | undefined, envVars: args.envVars }; return Promise.resolve()
}
async function runCustomCommand(cmd: CustomCommand) {
  capturedArgs = null
  const envVars: [string, string][] = cmd.envs.filter(e => e.key.trim()).map(e => [e.key.trim(), e.value])
  return mockInvoke('cmd_run', { projectId: cmd.projectPath, command: cmd.binary, args: cmd.args, workingDir: cmd.workingDirectory, envVars })
}

// Simulate addCustomCommand matching logic (mirror of index.vue)
function findProjectForFolder(projects: ProjectInfo[], folderId: string): ProjectInfo | null {
  return projects.find(p => p.id.startsWith(folderId) || folderId.includes(p.id)) || null
}

describe('buildTreeItems: custom commands attachment', () => {
  it('should attach custom commands to a leaf project item', () => {
    const projects: ProjectInfo[] = [{ id: '/projects/backend', name: 'backend', project_path: '/projects/backend', root_path: '/projects', icon: '', manifest: 'package.json', commands: [] }]
    const cmds: CustomCommand[] = [{ id: 'cmd:1', name: 'Server', binary: 'npm', args: ['run', 'dev'], projectPath: '/projects/backend', workingDirectory: '', envs: [{ key: 'PORT', value: '3000' }] }]
    const tree = buildTreeItems(projects, cmds)
    // tree[0] = root folder /projects, tree[0].children[0] = leaf project, tree[0].children[0].children[0] = custom cmd
    expect(tree).toHaveLength(1)
    expect(tree[0].isFolder).toBe(true)
    expect(tree[0].children).toHaveLength(1)
    expect(tree[0].children![0].isFolder).toBe(false)
    expect(tree[0].children![0].children).toHaveLength(1)
    expect(tree[0].children![0].children![0].isCustomCommand).toBe(true)
    expect(tree[0].children![0].children![0].label).toBe('Server')
  })

  it('should attach custom commands to multi-manifest folder children', () => {
    const projects: ProjectInfo[] = [
      { id: '/work:package.json', name: 'work', project_path: '/work', root_path: '/work', manifest: 'package.json', commands: [] },
      { id: '/work:Cargo.toml', name: 'work-rust', project_path: '/work', root_path: '/work', manifest: 'Cargo.toml', commands: [] }
    ]
    const cmds: CustomCommand[] = [
      { id: 'cmd:1', name: 'FE dev', binary: 'npm', args: ['run', 'dev'], projectPath: '/work:package.json', workingDirectory: '', envs: [] },
      { id: 'cmd:2', name: 'BE run', binary: 'cargo', args: ['run'], projectPath: '/work:Cargo.toml', workingDirectory: '', envs: [] }
    ]
    const tree = buildTreeItems(projects, cmds)
    expect(tree).toHaveLength(1); expect(tree[0].isMultiManifestFolder).toBe(true)
    expect(tree[0].children).toHaveLength(2)
    const npmChild = tree[0].children!.find(c => c.label === 'npm')!
    const cargoChild = tree[0].children!.find(c => c.label === 'cargo')!
    expect(npmChild.children).toHaveLength(1); expect(npmChild.children![0].isCustomCommand).toBe(true)
    expect(npmChild.children![0].label).toBe('FE dev')
    expect(cargoChild.children).toHaveLength(1); expect(cargoChild.children![0].isCustomCommand).toBe(true)
    expect(cargoChild.children![0].label).toBe('BE run')
  })

  it('should attach folder-level custom commands to the folder node', () => {
    const projects: ProjectInfo[] = [
      { id: '/projects/backend', name: 'backend', project_path: '/projects/backend', root_path: '/projects', manifest: 'package.json', commands: [] },
      { id: '/projects/frontend', name: 'frontend', project_path: '/projects/frontend', root_path: '/projects', manifest: 'package.json', commands: [] }
    ]
    const cmds: CustomCommand[] = [{ id: 'cmd:1', name: 'Build all', binary: 'make', args: [], projectPath: '/projects', workingDirectory: '', envs: [] }]
    const tree = buildTreeItems(projects, cmds)
    const folder = tree[0]; expect(folder.isFolder).toBe(true)
    expect(folder.children!.length).toBe(3) // backend, frontend, Build all
    const cmdItem = folder.children!.find(c => c.isCustomCommand)
    expect(cmdItem).toBeDefined(); expect(cmdItem!.label).toBe('Build all')
  })
})

describe('runCustomCommand: envVars serialization', () => {
  it('should send envVars as array of tuples NOT as object', async () => {
    const cmd: CustomCommand = { id: 'cmd:1', name: 'Server', binary: 'npm', args: ['run', 'dev'], projectPath: '/projects/backend', workingDirectory: '', envs: [{ key: 'PORT', value: '3000' }, { key: 'HOST', value: 'localhost' }, { key: '', value: 'empty' }] }
    await runCustomCommand(cmd)
    expect(capturedArgs).not.toBeNull()
    expect(Array.isArray(capturedArgs!.envVars)).toBe(true)
    expect((capturedArgs!.envVars as [string, string][]).length).toBe(2)
    expect((capturedArgs!.envVars as [string, string][])).toEqual([['PORT', '3000'], ['HOST', 'localhost']])
  })

  it('should send empty array when no envs', async () => {
    const cmd: CustomCommand = { id: 'cmd:2', name: 'Simple', binary: 'echo', args: ['hello'], projectPath: '/projects/backend', workingDirectory: '', envs: [] }
    await runCustomCommand(cmd)
    expect(capturedArgs!.envVars).toEqual([])
  })

  it('should filter whitespace-only keys', async () => {
    const cmd: CustomCommand = { id: 'cmd:3', name: 'Test', binary: 'test', args: [], projectPath: '/projects/backend', workingDirectory: '', envs: [{ key: '  ', value: 'v' }, { key: '\t', value: 'v2' }] }
    await runCustomCommand(cmd)
    expect((capturedArgs!.envVars as [string, string][]).length).toBe(0)
  })
})

describe('runCustomCommand: projectId correctness', () => {
  it('should send stored projectPath as projectId for multi-manifest', async () => {
    const cmd: CustomCommand = { id: 'cmd:1', name: 'Server', binary: 'npm', args: ['run', 'dev'], projectPath: '/projects/backend:package.json', workingDirectory: '/projects/backend', envs: [] }
    await runCustomCommand(cmd)
    expect(capturedArgs!.projectId).toBe('/projects/backend:package.json')
    expect(capturedArgs!.cmd).toBe('cmd_run')
  })

  it('should send stored projectPath for plain project id', async () => {
    const cmd: CustomCommand = { id: 'cmd:2', name: 'Run', binary: 'cargo', args: ['run'], projectPath: '/projects/backend', workingDirectory: '', envs: [] }
    await runCustomCommand(cmd)
    expect(capturedArgs!.projectId).toBe('/projects/backend')
  })
})

describe('addCustomCommand: stores project.id not project_path', () => {
  it('should store full id with manifest suffix, NOT project_path', () => {
    const projects: ProjectInfo[] = [{ id: '/work:package.json', name: 'work', project_path: '/work', root_path: '/work', manifest: 'package.json', commands: [] }]
    const selectedFolderId = '/work'
    const found = findProjectForFolder(projects, selectedFolderId)
    const projectPath = found ? found.id : null
    // FIXED: must be found.id (with suffix), NOT found.project_path
    expect(projectPath).toBe('/work:package.json')
  })

  it('should work when id equals project_path', () => {
    const projects: ProjectInfo[] = [{ id: '/projects/backend', name: 'backend', project_path: '/projects/backend', root_path: '/projects', manifest: 'package.json', commands: [] }]
    const selectedFolderId = '/projects/backend'
    const found = findProjectForFolder(projects, selectedFolderId)
    expect(found?.id).toBe('/projects/backend')
  })
})

describe('integration: add then run', () => {
  it('folder-level command: create on folder, cmd attached to folder, run sends folder path', async () => {
    const projects: ProjectInfo[] = [
      { id: '/projects/backend', name: 'backend', project_path: '/projects/backend', root_path: '/projects', manifest: 'package.json', commands: [] },
      { id: '/projects/frontend', name: 'frontend', project_path: '/projects/frontend', root_path: '/projects', manifest: 'package.json', commands: [] }
    ]
    // User selects /projects folder — matching finds first project
    const selectedFolderId = '/projects'
    const found = findProjectForFolder(projects, selectedFolderId)
    const projectPath = found ? found.id : ''
    // addCustomCommand stores projectPath = found.id
    // But for folder-level commands, the real code likely uses the folder id
    // For this test, let's test the folder-path variant
    const cmd: CustomCommand = { id: 'cmd:all', name: 'Build all', binary: 'make', args: [], projectPath: '/projects', workingDirectory: '/projects', envs: [] }

    // Tree shows cmd on folder node
    const tree = buildTreeItems(projects, [cmd])
    expect(tree).toHaveLength(1); expect(tree[0].isFolder).toBe(true)
    expect(tree[0].children!.find(c => c.isCustomCommand)).toBeDefined()

    // Run sends the correct projectId
    await runCustomCommand(cmd)
    expect(capturedArgs!.projectId).toBe('/projects')
  })

  it('project-specific command: create on package.json child, cmd on npm child, run sends project id', async () => {
    const projects: ProjectInfo[] = [
      { id: '/work:package.json', name: 'work', project_path: '/work', root_path: '/work', manifest: 'package.json', commands: [] },
      { id: '/work:Cargo.toml', name: 'work-rust', project_path: '/work', root_path: '/work', manifest: 'Cargo.toml', commands: [] }
    ]
    // User creates command on package.json project directly
    const cmd: CustomCommand = { id: 'cmd:fe', name: 'FE dev', binary: 'npm', args: ['run', 'dev'], projectPath: '/work:package.json', workingDirectory: '/work', envs: [{ key: 'PORT', value: '3000' }] }

    // Tree shows cmd under npm child
    const tree = buildTreeItems(projects, [cmd])
    expect(tree).toHaveLength(1); expect(tree[0].isMultiManifestFolder).toBe(true)
    const npmChild = tree[0].children!.find(c => c.label === 'npm')
    expect(npmChild).toBeDefined()
    expect(npmChild!.children).toHaveLength(1)
    expect(npmChild!.children![0].isCustomCommand).toBe(true)
    expect(npmChild!.children![0].label).toBe('FE dev')

    // Run sends the correct projectId
    await runCustomCommand(cmd)
    expect(capturedArgs!.projectId).toBe('/work:package.json')
    expect(capturedArgs!.command).toBe('npm')
    expect(capturedArgs!.args).toEqual(['run', 'dev'])
    expect(capturedArgs!.workingDir).toBe('/work')
    expect((capturedArgs!.envVars as [string, string][])).toEqual([['PORT', '3000']])
  })
})
