<template>
  <div class="fe">

    <!-- ── Estado inicial: drag & drop / botón ──────── -->
    <div v-if="!vfsLoaded" class="fe-drop" @dragover.prevent @drop.prevent="onDrop">
      <div class="fe-drop-inner">
        <div class="fe-drop-icon">📁</div>
        <p class="fe-drop-title">Arrastra una carpeta aquí</p>
        <p class="fe-drop-sub">o</p>
        <label class="fe-drop-btn">
          Seleccionar carpeta
          <input type="file" webkitdirectory multiple @change="onFileInput" />
        </label>
        <p class="fe-drop-hint">Al cargar una carpeta, la terminal también tendrá acceso a sus archivos.</p>
      </div>
    </div>

    <!-- ── Explorador de archivos ────────────────────── -->
    <template v-else>

      <!-- Toolbar -->
      <div class="fe-toolbar">
        <button class="fe-tb-btn" :disabled="historyIdx <= 0"                   title="Atrás"      @click="goBack">‹</button>
        <button class="fe-tb-btn" :disabled="historyIdx >= history.length - 1"  title="Adelante"   @click="goForward">›</button>
        <button class="fe-tb-btn" :disabled="cwd === '/'"                        title="Subir nivel" @click="goUp">↑</button>

        <!-- Breadcrumb -->
        <div class="fe-breadcrumb">
          <span
            v-for="(crumb, i) in breadcrumb"
            :key="crumb.path"
            class="fe-crumb"
            :class="{ 'fe-crumb--last': i === breadcrumb.length - 1 }"
            @click="i < breadcrumb.length - 1 && navigateTo(crumb.path)"
          >
            {{ crumb.name }}
            <span v-if="i < breadcrumb.length - 1" class="fe-sep">›</span>
          </span>
        </div>

        <!-- Búsqueda -->
        <input v-model="search" class="fe-search" placeholder="Buscar..." />

        <!-- Vista grid/lista -->
        <button class="fe-tb-btn" :title="viewMode === 'grid' ? 'Vista lista' : 'Vista cuadrícula'" @click="toggleView">
          {{ viewMode === 'grid' ? '☰' : '⊞' }}
        </button>

        <!-- Abrir nueva carpeta -->
        <label class="fe-tb-btn" title="Cargar otra carpeta">
          📂
          <input type="file" webkitdirectory multiple @change="onFileInput" />
        </label>

        <!-- Descargar / reset -->
        <button class="fe-tb-btn" title="Quitar carpeta" @click="onReset">✕</button>
      </div>

      <!-- Body: sidebar + content -->
      <div class="fe-body">

        <!-- Sidebar: árbol -->
        <div class="fe-sidebar">
          <div class="fe-sidebar-title">CARPETAS</div>
          <TreeNode
            :node="vfsRoot"
            path="/"
            :activePath="cwd"
            @select="navigateTo"
          />
        </div>

        <!-- Contenido principal -->
        <div class="fe-content" @click.self="selectedFile = null">

          <div v-if="filteredChildren.length === 0" class="fe-empty">
            <span>📂</span>
            <p>{{ search ? 'Sin resultados' : 'Carpeta vacía' }}</p>
          </div>

          <!-- Vista cuadrícula -->
          <div v-else-if="viewMode === 'grid'" class="fe-grid">
            <div
              v-for="item in filteredChildren"
              :key="item.name"
              class="fe-item fe-item--grid"
              :class="{
                'fe-item--selected': selectedFile === item.name,
                'fe-item--folder':   item.type === 'dir',
              }"
              @click.stop="selectedFile = item.name"
              @dblclick="item.type === 'dir' && navigateTo(childPath(item.name))"
            >
              <span class="fe-item-icon">{{ fileIcon(item) }}</span>
              <span class="fe-item-name">{{ item.name }}</span>
            </div>
          </div>

          <!-- Vista lista -->
          <table v-else class="fe-table">
            <thead>
              <tr>
                <th @click="sortBy('name')" class="fe-th">Nombre <span v-if="sort.key==='name'">{{ sort.asc ? '↑' : '↓' }}</span></th>
                <th @click="sortBy('type')" class="fe-th">Tipo</th>
                <th @click="sortBy('size')" class="fe-th">Tamaño <span v-if="sort.key==='size'">{{ sort.asc ? '↑' : '↓' }}</span></th>
              </tr>
            </thead>
            <tbody>
              <tr
                v-for="item in filteredChildren"
                :key="item.name"
                class="fe-row"
                :class="{
                  'fe-row--selected': selectedFile === item.name,
                  'fe-row--folder':   item.type === 'dir',
                }"
                @click.stop="selectedFile = item.name"
                @dblclick="item.type === 'dir' && navigateTo(childPath(item.name))"
              >
                <td class="fe-td fe-td-name">
                  <span class="fe-item-icon-sm">{{ fileIcon(item) }}</span>
                  {{ item.name }}
                </td>
                <td class="fe-td">{{ item.type === 'dir' ? 'Carpeta' : fileExt(item.name) }}</td>
                <td class="fe-td">{{ item.type === 'dir' ? `${Object.keys(item.children).length} elementos` : formatSize(item.size) }}</td>
              </tr>
            </tbody>
          </table>
        </div>
      </div>

      <!-- Status bar -->
      <div class="fe-statusbar">
        <span>{{ filteredChildren.length }} elementos</span>
        <span v-if="selectedItem"> · {{ selectedItem.name }}{{ selectedItem.size != null ? ' — ' + formatSize(selectedItem.size) : '' }}</span>
        <span class="fe-statusbar-hint">Sincronizado con la terminal</span>
      </div>

    </template>
  </div>
</template>

<script setup>
import { ref, computed, watch, defineComponent, h } from 'vue'
import {
  vfsRoot, cwd, vfsLoaded,
  loadFromFiles, resetVfs,
  pathBreadcrumb, getNode, resolvePath,
} from '../store/vfs.js'

/* ── TreeNode recursivo ──────────────────────────────────────── */
const TreeNode = defineComponent({
  name: 'TreeNode',
  props: {
    node:       { type: Object,  required: true },
    path:       { type: String,  required: true },
    activePath: { type: String,  default: '' },
  },
  emits: ['select'],
  setup(props, { emit }) {
    const open = ref(props.path === '/')

    const dirs = computed(() =>
      Object.values(props.node.children ?? {}).filter(c => c.type === 'dir')
    )

    // Abrir automáticamente si el cwd está dentro de este nodo
    watch(() => props.activePath, (ap) => {
      if (ap.startsWith(props.path + '/') || ap === props.path) open.value = true
    }, { immediate: true })

    return () => {
      const isActive = props.activePath === props.path

      return h('div', { class: 'tn' }, [
        h('div', {
          class: ['tn-row', isActive && 'tn-row--active'],
          onClick: () => emit('select', props.path),
        }, [
          dirs.value.length
            ? h('span', {
                class: ['tn-arrow', open.value && 'tn-arrow--open'],
                onClick: (e) => { e.stopPropagation(); open.value = !open.value },
              }, '›')
            : h('span', { class: 'tn-arrow tn-arrow--leaf' }),
          h('span', { class: 'tn-icon' }, '📁'),
          h('span', { class: 'tn-label' }, props.node.name),
        ]),
        open.value && dirs.value.length
          ? h('div', { class: 'tn-children' },
              dirs.value.map(child => {
                const childPath = props.path === '/' ? '/' + child.name : props.path + '/' + child.name
                return h(TreeNode, {
                  key:        child.name,
                  node:       child,
                  path:       childPath,
                  activePath: props.activePath,
                  onSelect:   (p) => emit('select', p),
                })
              })
            )
          : null,
      ])
    }
  },
})

/* ── Estado local del explorador ─────────────────────────────── */
const history    = ref(['/'])
const historyIdx = ref(0)
const selectedFile = ref(null)
const search     = ref('')
const viewMode   = ref('grid')
const sort       = ref({ key: 'name', asc: true })

/* ── Sincronización bidireccional con cwd del store ──────────── */
// Cuando la terminal hace cd, el explorador sigue
watch(cwd, (newPath) => {
  // Solo actualizar si es diferente al último en historial
  if (history.value[historyIdx.value] !== newPath) {
    history.value = history.value.slice(0, historyIdx.value + 1)
    history.value.push(newPath)
    historyIdx.value = history.value.length - 1
    selectedFile.value = null
    search.value = ''
  }
})

/* ── Nodo actual derivado del store ──────────────────────────── */
const currentNode = computed(() => getNode(cwd.value) ?? vfsRoot)

/* ── Breadcrumb calculado desde el path string ───────────────── */
const breadcrumb = computed(() => pathBreadcrumb(cwd.value))

/* ── Hijos filtrados y ordenados ─────────────────────────────── */
const filteredChildren = computed(() => {
  const children = Object.values(currentNode.value.children ?? {})
  let items = search.value.trim()
    ? children.filter(i => i.name.toLowerCase().includes(search.value.toLowerCase()))
    : children

  return [...items].sort((a, b) => {
    if (a.type !== b.type) return a.type === 'dir' ? -1 : 1
    const k  = sort.value.key
    let va   = a[k] ?? '', vb = b[k] ?? ''
    if (typeof va === 'string') va = va.toLowerCase()
    if (typeof vb === 'string') vb = vb.toLowerCase()
    if (va < vb) return sort.value.asc ? -1 : 1
    if (va > vb) return sort.value.asc ? 1 : -1
    return 0
  })
})

const selectedItem = computed(() =>
  selectedFile.value
    ? (currentNode.value?.children ?? {})[selectedFile.value]
    : null
)

/* ── Helpers ─────────────────────────────────────────────────── */
function childPath(name) {
  return cwd.value === '/' ? '/' + name : cwd.value + '/' + name
}

/* ── Navegación ──────────────────────────────────────────────── */
function navigateTo(path) {
  const node = getNode(path)
  if (!node || node.type !== 'dir') return

  history.value = history.value.slice(0, historyIdx.value + 1)
  history.value.push(path)
  historyIdx.value = history.value.length - 1

  cwd.value = path   // ← actualiza el store compartido (la terminal también ve el cambio)
  selectedFile.value = null
  search.value = ''
}

function goBack() {
  if (historyIdx.value <= 0) return
  historyIdx.value--
  cwd.value = history.value[historyIdx.value]
  selectedFile.value = null
}

function goForward() {
  if (historyIdx.value >= history.value.length - 1) return
  historyIdx.value++
  cwd.value = history.value[historyIdx.value]
  selectedFile.value = null
}

function goUp() {
  if (cwd.value === '/') return
  const parts = cwd.value.split('/').filter(Boolean)
  parts.pop()
  navigateTo(parts.length ? '/' + parts.join('/') : '/')
}

/* ── Carga de archivos ───────────────────────────────────────── */
async function onFileInput(e) {
  await loadFromFiles(e.target.files)
  history.value = ['/']
  historyIdx.value = 0
}

async function onDrop(e) {
  const items = e.dataTransfer?.items
  if (items) {
    const files = []
    const promises = []
    for (const item of items) {
      const entry = item.webkitGetAsEntry?.()
      if (entry) promises.push(readEntry(entry, '', files))
    }
    await Promise.all(promises)
    if (files.length) {
      await loadFromFiles(files)
      history.value = ['/']
      historyIdx.value = 0
    }
  } else {
    await loadFromFiles(e.dataTransfer?.files)
  }
}

async function readEntry(entry, path, files) {
  if (entry.isFile) {
    await new Promise(resolve => {
      entry.file(file => {
        Object.defineProperty(file, 'webkitRelativePath', { value: path + file.name })
        files.push(file)
        resolve()
      })
    })
  } else if (entry.isDirectory) {
    const reader = entry.createReader()
    await new Promise(resolve => {
      const readAll = () => {
        reader.readEntries(async entries => {
          if (!entries.length) { resolve(); return }
          for (const e of entries) await readEntry(e, path + entry.name + '/', files)
          readAll()
        })
      }
      readAll()
    })
  }
}

function onReset() {
  resetVfs()
  history.value = ['/']
  historyIdx.value = 0
  selectedFile.value = null
  search.value = ''
}

/* ── UI helpers ──────────────────────────────────────────────── */
function toggleView() { viewMode.value = viewMode.value === 'grid' ? 'list' : 'grid' }

function sortBy(key) {
  if (sort.value.key === key) sort.value.asc = !sort.value.asc
  else { sort.value.key = key; sort.value.asc = true }
}

function fileExt(name) {
  const dot = name.lastIndexOf('.')
  return dot > 0 ? name.slice(dot + 1).toUpperCase() : 'Archivo'
}

function formatSize(bytes) {
  if (bytes == null) return ''
  if (bytes < 1024)           return bytes + ' B'
  if (bytes < 1024 * 1024)    return (bytes / 1024).toFixed(1) + ' KB'
  return (bytes / (1024 * 1024)).toFixed(1) + ' MB'
}

const EXT_ICONS = {
  png:'🖼', jpg:'🖼', jpeg:'🖼', gif:'🖼', svg:'🖼', webp:'🖼', bmp:'🖼', ico:'🖼',
  pdf:'📄', doc:'📝', docx:'📝', xls:'📊', xlsx:'📊', ppt:'📋', pptx:'📋', txt:'📝', md:'📝',
  js:'📜', ts:'📜', jsx:'📜', tsx:'📜', vue:'💚', py:'🐍', cpp:'⚙', c:'⚙', h:'⚙',
  java:'☕', go:'🐹', rs:'🦀', php:'🐘', rb:'💎', cs:'🔷', swift:'🔶',
  html:'🌐', css:'🎨', scss:'🎨', sass:'🎨', less:'🎨',
  json:'🔣', yaml:'🔣', yml:'🔣', xml:'🔣', env:'🔒', toml:'🔣',
  zip:'📦', tar:'📦', gz:'📦', rar:'📦', '7z':'📦',
  mp3:'🎵', wav:'🎵', mp4:'🎬', mov:'🎬', avi:'🎬',
  exe:'⚡', sh:'📟', bat:'📟', ps1:'📟',
}

function fileIcon(item) {
  if (item.type === 'dir') return '📁'
  const ext = item.name.split('.').pop()?.toLowerCase()
  return EXT_ICONS[ext] ?? '📄'
}
</script>

<style scoped>
.fe {
  display: flex;
  flex-direction: column;
  height: 100%;
  font-family: 'Segoe UI', system-ui, sans-serif;
  font-size: 0.8rem;
  color: #c9d1d9;
  background: #0d1117;
  overflow: hidden;
}

/* ── Drop zone ─────────────────────────────────────── */
.fe-drop {
  flex: 1;
  display: flex;
  align-items: center;
  justify-content: center;
  background:
    repeating-linear-gradient(
      45deg,
      rgba(48,54,61,0.18) 0,
      rgba(48,54,61,0.18) 1px,
      transparent 0,
      transparent 50%
    );
  background-size: 12px 12px;
}

.fe-drop-inner {
  display: flex;
  flex-direction: column;
  align-items: center;
  gap: 10px;
  padding: 36px 40px;
  border: 2px dashed #30363d;
  border-radius: 16px;
  background: rgba(22,27,34,0.9);
  text-align: center;
  transition: border-color 0.2s;
}
.fe-drop-inner:hover { border-color: #f0883e; }

.fe-drop-icon  { font-size: 3rem; }
.fe-drop-title { font-size: 0.95rem; font-weight: 600; color: #e6edf3; margin: 0; }
.fe-drop-sub   { font-size: 0.75rem; color: #484f58; margin: 0; }
.fe-drop-hint  { font-size: 0.68rem; color: #484f58; margin: 0; max-width: 240px; line-height: 1.4; }

.fe-drop-btn {
  display: inline-block;
  padding: 6px 18px;
  background: #21262d;
  border: 1px solid #30363d;
  border-radius: 8px;
  color: #f0883e;
  cursor: pointer;
  font-size: 0.78rem;
  font-family: inherit;
  transition: background 0.15s, border-color 0.15s;
}
.fe-drop-btn:hover { background: #30363d; border-color: #f0883e; }
.fe-drop-btn input { display: none; }

/* ── Toolbar ───────────────────────────────────────── */
.fe-toolbar {
  display: flex;
  align-items: center;
  gap: 4px;
  padding: 4px 8px;
  background: #161b22;
  border-bottom: 1px solid #21262d;
  flex-shrink: 0;
  min-height: 34px;
}

.fe-tb-btn {
  display: flex;
  align-items: center;
  justify-content: center;
  min-width: 26px;
  height: 26px;
  padding: 0 6px;
  background: transparent;
  border: 1px solid transparent;
  border-radius: 5px;
  color: #8b949e;
  font-size: 0.85rem;
  cursor: pointer;
  font-family: inherit;
  transition: background 0.12s, border-color 0.12s, color 0.12s;
  white-space: nowrap;
}
.fe-tb-btn:hover:not(:disabled) { background: #21262d; border-color: #30363d; color: #e6edf3; }
.fe-tb-btn:disabled { opacity: 0.3; cursor: default; }
.fe-tb-btn input { display: none; }

.fe-breadcrumb {
  flex: 1;
  display: flex;
  align-items: center;
  overflow: hidden;
  background: #0d1117;
  border: 1px solid #21262d;
  border-radius: 5px;
  padding: 0 8px;
  height: 26px;
  gap: 2px;
  min-width: 0;
}

.fe-crumb {
  display: flex;
  align-items: center;
  gap: 2px;
  font-size: 0.72rem;
  color: #f0883e;
  cursor: pointer;
  white-space: nowrap;
  transition: color 0.12s;
}
.fe-crumb:hover { color: #ffa657; }
.fe-crumb--last { color: #e6edf3; cursor: default; pointer-events: none; }
.fe-sep { color: #484f58; margin: 0 2px; }

.fe-search {
  width: 130px;
  height: 26px;
  background: #0d1117;
  border: 1px solid #21262d;
  border-radius: 5px;
  padding: 0 8px;
  color: #c9d1d9;
  font-size: 0.72rem;
  font-family: inherit;
  outline: none;
  transition: border-color 0.15s;
  flex-shrink: 0;
}
.fe-search:focus { border-color: #f0883e; }
.fe-search::placeholder { color: #484f58; }

/* ── Body ──────────────────────────────────────────── */
.fe-body {
  flex: 1;
  display: flex;
  overflow: hidden;
  min-height: 0;
}

/* ── Sidebar ───────────────────────────────────────── */
.fe-sidebar {
  width: 180px;
  flex-shrink: 0;
  background: #0d1117;
  border-right: 1px solid #21262d;
  overflow-y: auto;
  padding: 8px 0;
}
.fe-sidebar::-webkit-scrollbar { width: 4px; }
.fe-sidebar::-webkit-scrollbar-thumb { background: #30363d; border-radius: 2px; }

.fe-sidebar-title {
  font-size: 0.6rem;
  letter-spacing: 0.08em;
  color: #484f58;
  padding: 0 12px 6px;
  font-weight: 600;
}

:deep(.tn) { user-select: none; }
:deep(.tn-row) {
  display: flex;
  align-items: center;
  gap: 4px;
  padding: 3px 10px 3px 8px;
  cursor: pointer;
  border-radius: 4px;
  margin: 1px 4px;
  transition: background 0.1s;
  font-size: 0.75rem;
  color: #8b949e;
}
:deep(.tn-row:hover)       { background: #161b22; color: #c9d1d9; }
:deep(.tn-row--active)     { background: #1f2937; color: #f0883e !important; }
:deep(.tn-arrow) {
  width: 12px; font-size: 0.7rem; color: #484f58;
  transition: transform 0.15s; display: inline-block;
  text-align: center; flex-shrink: 0;
}
:deep(.tn-arrow--open)     { transform: rotate(90deg); }
:deep(.tn-arrow--leaf)     { visibility: hidden; }
:deep(.tn-icon)            { font-size: 0.8rem; flex-shrink: 0; }
:deep(.tn-label)           { overflow: hidden; text-overflow: ellipsis; white-space: nowrap; flex: 1; }
:deep(.tn-children)        { padding-left: 14px; }

/* ── Contenido ─────────────────────────────────────── */
.fe-content {
  flex: 1;
  overflow-y: auto;
  padding: 12px;
  min-width: 0;
}
.fe-content::-webkit-scrollbar { width: 6px; }
.fe-content::-webkit-scrollbar-thumb { background: #21262d; border-radius: 3px; }
.fe-content::-webkit-scrollbar-thumb:hover { background: #30363d; }

.fe-empty {
  display: flex;
  flex-direction: column;
  align-items: center;
  justify-content: center;
  height: 120px;
  color: #484f58;
  font-size: 0.78rem;
  gap: 6px;
}
.fe-empty span { font-size: 2rem; }

/* Grid */
.fe-grid {
  display: grid;
  grid-template-columns: repeat(auto-fill, minmax(80px, 1fr));
  gap: 4px;
}

.fe-item--grid {
  display: flex;
  flex-direction: column;
  align-items: center;
  gap: 5px;
  padding: 10px 6px 8px;
  border-radius: 6px;
  cursor: pointer;
  border: 1px solid transparent;
  transition: background 0.1s, border-color 0.1s;
  text-align: center;
}
.fe-item--grid:hover                   { background: #161b22; }
.fe-item--grid.fe-item--selected       { background: #1f2937; border-color: #f0883e44; }

.fe-item-icon { font-size: 2rem; display: block; }
.fe-item-name {
  font-size: 0.67rem;
  color: #c9d1d9;
  word-break: break-word;
  line-height: 1.3;
  max-height: 2.6em;
  overflow: hidden;
  display: -webkit-box;
  -webkit-line-clamp: 2;
  -webkit-box-orient: vertical;
}

/* Tabla */
.fe-table { width: 100%; border-collapse: collapse; font-size: 0.75rem; }

.fe-th {
  text-align: left;
  padding: 5px 10px;
  color: #484f58;
  font-weight: 500;
  border-bottom: 1px solid #21262d;
  cursor: pointer;
  user-select: none;
  font-size: 0.7rem;
  white-space: nowrap;
}
.fe-th:hover { color: #8b949e; }

.fe-row { cursor: pointer; transition: background 0.08s; }
.fe-row:hover           { background: #161b22; }
.fe-row--selected       { background: #1f2937; }

.fe-td { padding: 4px 10px; color: #8b949e; white-space: nowrap; border-bottom: 1px solid #0d1117; }
.fe-td-name {
  display: flex;
  align-items: center;
  gap: 6px;
  color: #c9d1d9;
  max-width: 220px;
  overflow: hidden;
  text-overflow: ellipsis;
}
.fe-item-icon-sm { font-size: 1rem; flex-shrink: 0; }

/* ── Status bar ────────────────────────────────────── */
.fe-statusbar {
  padding: 3px 12px;
  background: #161b22;
  border-top: 1px solid #21262d;
  font-size: 0.67rem;
  color: #484f58;
  flex-shrink: 0;
  display: flex;
  gap: 4px;
  align-items: center;
}

.fe-statusbar-hint {
  margin-left: auto;
  color: #f0883e55;
  font-style: italic;
}
</style>
