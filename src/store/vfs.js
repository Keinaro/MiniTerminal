import { reactive, ref } from 'vue'

// ── VFS Root ──────────────────────────────────────────────────
// Nodo raíz compartido entre terminal y explorador.
// Estructura de nodo: { type, name, children, content, size, modified }
export const vfsRoot = reactive({
  type: 'dir',
  name: '/',
  children: {},
  modified: '',
})

// Directorio de trabajo compartido (path string, ej: "/src/components")
export const cwd = ref('/')

// Nombre de usuario editable (compartido entre header y terminal)
export const username = ref('usuario')

// Flag: se subió alguna carpeta
export const vfsLoaded = ref(false)

// ── Helpers de ruta ───────────────────────────────────────────
export function resolvePath(base, target) {
  if (!target || target === '.') return base
  if (target === '~') return '/'
  if (target === '..') {
    const parts = base.split('/').filter(Boolean)
    parts.pop()
    return '/' + parts.join('/')
  }
  if (target.startsWith('~/')) return '/' + target.slice(2)
  if (target.startsWith('/'))  return target
  if (target.startsWith('../')) {
    const parts = base.split('/').filter(Boolean)
    parts.pop()
    return resolvePath('/' + parts.join('/'), target.slice(3))
  }
  return base === '/' ? '/' + target : base + '/' + target
}

export function getNode(path) {
  if (path === '/' || path === '') return vfsRoot
  const parts = path.split('/').filter(Boolean)
  let node = vfsRoot
  for (const part of parts) {
    if (!node || node.type !== 'dir' || !node.children[part]) return null
    node = node.children[part]
  }
  return node
}

// Resolución case-insensitive: devuelve { path, node } con el casing real del árbol
export function resolveCI(base, target) {
  const rawPath = resolvePath(base, target)
  // Intentar match exacto primero
  const exact = getNode(rawPath)
  if (exact) return { path: rawPath === '' ? '/' : rawPath, node: exact }
  // Match case-insensitive segmento a segmento
  const parts = rawPath.split('/').filter(Boolean)
  let node = vfsRoot
  let resolved = ''
  for (const part of parts) {
    if (!node || node.type !== 'dir') return null
    const lower = part.toLowerCase()
    const entry = Object.entries(node.children).find(([k]) => k.toLowerCase() === lower)
    if (!entry) return null
    node = entry[1]
    resolved += '/' + entry[0]
  }
  return { path: resolved || '/', node }
}

export function getParentAndName(path) {
  const parts = path.split('/').filter(Boolean)
  const name = parts.pop()
  const parentPath = '/' + parts.join('/')
  return { parent: getNode(parentPath), name, parentPath }
}

// Devuelve el path de un nodo subiendo por 'breadcrumb' (para el explorador)
// como no guardamos refs a parent en el árbol, lo calculamos desde el path string
export function pathBreadcrumb(pathStr) {
  if (pathStr === '/') return [{ name: vfsRoot.name || '/', path: '/' }]
  const parts = pathStr.split('/').filter(Boolean)
  const crumbs = [{ name: vfsRoot.name || '/', path: '/' }]
  let acc = ''
  for (const p of parts) {
    acc += '/' + p
    crumbs.push({ name: p, path: acc })
  }
  return crumbs
}

// ── Cargar desde FileList (webkitdirectory) ───────────────────
const TEXT_EXTS = new Set([
  'txt','md','js','ts','jsx','tsx','vue','py','cpp','c','h','hpp','java',
  'go','rs','rb','php','cs','swift','html','htm','css','scss','sass','less',
  'json','yaml','yml','xml','toml','env','sh','bash','zsh','bat','ps1',
  'sql','graphql','gql','gitignore','dockerfile','makefile','cmake','ini',
  'cfg','conf','log','csv','tsv','r','lua','pl','scala','kt','dart',
])

function isText(name) {
  const dot = name.lastIndexOf('.')
  if (dot < 0) return true  // sin extensión → posiblemente texto
  return TEXT_EXTS.has(name.slice(dot + 1).toLowerCase())
}

export async function loadFromFiles(fileList) {
  const files = Array.from(fileList)
  if (!files.length) return

  // Detectar el nombre de la carpeta raíz desde webkitRelativePath
  const rootName = files[0].webkitRelativePath.split('/')[0] || 'carpeta'

  // Resetear árbol
  vfsRoot.name = rootName
  vfsRoot.children = {}
  vfsRoot.modified = fmt(Date.now())

  for (const file of files) {
    const parts = file.webkitRelativePath.split('/')
    let node = vfsRoot

    // parts[0] es la carpeta raíz — ya es vfsRoot, empezamos desde [1]
    for (let i = 1; i < parts.length; i++) {
      const part = parts[i]
      const isLast = i === parts.length - 1

      if (isLast) {
        // Es un archivo
        let content = ''
        if (isText(part) && file.size < 512 * 1024) {
          try { content = await file.text() } catch { content = '' }
        }
        node.children[part] = {
          type: 'file',
          name: part,
          size: file.size,
          modified: fmt(file.lastModified),
          content,
        }
      } else {
        // Es un directorio intermedio
        if (!node.children[part]) {
          node.children[part] = {
            type: 'dir',
            name: part,
            children: {},
            modified: fmt(file.lastModified),
          }
        }
        node = node.children[part]
      }
    }
  }

  cwd.value = '/'
  vfsLoaded.value = true
}

function fmt(ts) {
  return new Date(ts).toLocaleString('es', {
    month: 'short', day: '2-digit',
    hour: '2-digit', minute: '2-digit',
  })
}

export function resetVfs() {
  vfsRoot.name = '/'
  vfsRoot.children = {}
  cwd.value = '/'
  vfsLoaded.value = false
}
