<template>
  <div class="terminal" @click="focusInput">
    <!-- Área de salida -->
    <div class="output" ref="outputRef">
      <div v-for="(line, i) in outputLines" :key="i">
        <span v-if="line.type === 'prompt'" class="line-prompt">
          <span class="prompt-path">{{ line.path }}&nbsp;</span>
          <span class="prompt-cmd">{{ line.text }}</span>
        </span>
        <span v-else-if="line.type === 'html'"  v-html="line.text" class="line-html"></span>
        <span v-else-if="line.type === 'error'" class="line-error">{{ line.text }}</span>
        <span v-else-if="line.type === 'info'"  class="line-info">{{ line.text }}</span>
        <span v-else-if="line.type === 'blank'" class="line-blank">&nbsp;</span>
        <span v-else class="line-plain">{{ line.text }}</span>
      </div>
    </div>

    <!-- Fila de entrada activa -->
    <div class="input-row" v-if="!processing">
      <span class="prompt-path">{{ promptLabel }}&nbsp;</span>
      <input
        ref="inputRef"
        v-model="currentInput"
        class="terminal-input"
        type="text"
        autocomplete="off"
        autocorrect="off"
        spellcheck="false"
        @keydown.enter="handleEnter"
        @keydown.up.prevent="historyUp"
        @keydown.down.prevent="historyDown"
        @keydown.tab.prevent="handleTab"
      />
    </div>
    <div class="input-row processing-row" v-else>
      <span class="processing-dots">ejecutando<span class="dot1">.</span><span class="dot2">.</span><span class="dot3">.</span></span>
    </div>
  </div>
</template>

<script setup>
import { ref, computed, nextTick, onMounted } from 'vue'
import {
  vfsRoot, cwd, vfsLoaded, username,
  resolvePath, resolveCI, getNode, getParentAndName,
} from '../store/vfs.js'

const emit = defineEmits(['process-event'])

/* ─── Prompt dinámico ─────────────────────────────────────────
   Muestra: usuario@miniterminal:/path$
   La raíz (/) se muestra como el nombre de la carpeta cargada. */
const promptLabel = computed(() => {
  const root = vfsRoot.name && vfsRoot.name !== '/' ? vfsRoot.name : '~'
  const path = cwd.value === '/' ? '' : cwd.value
  const display = root + path
  return `${username.value}@miniterminal:${display}$`
})

/* ─── Estado de la terminal ───────────────────────────────────── */
const outputLines  = ref([])
const currentInput = ref('')
const inputRef     = ref(null)
const outputRef    = ref(null)
const cmdHistory   = ref([])
const histIdx      = ref(-1)
const processing   = ref(false)
let pidCounter = 100

/* ─── Fase 4: Redirección E/S ─────────────────────────────────── */
function writeToFile(filePath, content, append = false) {
  const p = resolvePath(cwd.value, filePath)
  const { parent, name } = getParentAndName(p)
  if (!parent || parent.type !== 'dir') return false
  const ts = new Date().toLocaleString('es', { month: 'short', day: '2-digit', hour: '2-digit', minute: '2-digit' })
  if (parent.children[name]) {
    if (parent.children[name].type === 'dir') return false
    parent.children[name].content = append
      ? (parent.children[name].content || '') + '\n' + content
      : content
    parent.children[name].size = parent.children[name].content.length
    parent.children[name].modified = ts
  } else {
    parent.children[name] = { type: 'file', name, content, size: content.length, modified: ts }
  }
  return true
}

function extractText(result) {
  if (!result) return ''
  if (Array.isArray(result)) return result.map(extractText).filter(Boolean).join('\n')
  return (result.text || '').replace(/<br\s*\/?>/gi, '\n').replace(/<[^>]+>/g, '')
}

/* ─── Colorización ────────────────────────────────────────────── */
const EXT_COLORS = {
  cpp: '#7ee787', c: '#7ee787', h: '#7ee787',
  js: '#e3b341', ts: '#58a6ff', vue: '#56d364',
  md: '#e6edf3', txt: '#e6edf3', pdf: '#f85149',
  sh: '#ffa657', py: '#ffa657',
  json: '#79c0ff', yaml: '#79c0ff', yml: '#79c0ff',
}

function colorEntry(name, type) {
  if (type === 'dir') return `<span style="color:#58a6ff;font-weight:bold">${name}/</span>`
  const ext = name.split('.').pop().toLowerCase()
  const color = EXT_COLORS[ext] || '#c9d1d9'
  return `<span style="color:${color}">${name}</span>`
}

/* ─── Comandos ────────────────────────────────────────────────── */
const USERNAME = 'usuario'
const HOSTNAME  = 'miniterminal'

function cmdLs(args) {
  const flags   = args.filter(a => a.startsWith('-'))
  const paths   = args.filter(a => !a.startsWith('-'))
  const longFmt = flags.some(f => f.includes('l'))
  const showAll = flags.some(f => f.includes('a'))
  const target  = paths[0] ? resolvePath(cwd.value, paths[0]) : cwd.value
  const node    = getNode(target)
  if (!node) return { type: 'error', text: `ls: cannot access '${paths[0]}': No such file or directory` }
  if (node.type === 'file') return { type: 'html', text: colorEntry(node.name, 'file') }

  let entries = Object.values(node.children)
  if (!showAll) entries = entries.filter(e => !e.name.startsWith('.'))
  entries.sort((a, b) => {
    if (a.type !== b.type) return a.type === 'dir' ? -1 : 1
    return a.name.localeCompare(b.name)
  })

  if (!entries.length) return null

  if (longFmt) {
    const rows = entries.map(e => {
      const perm = e.type === 'dir' ? 'drwxr-xr-x' : '-rw-r--r--'
      const sz   = String(e.size || (e.type === 'dir' ? 4096 : 0)).padStart(6)
      const mod  = e.modified || 'Jan  1 00:00'
      return `${perm}  1 ${username.value} ${username.value} ${sz} ${mod} ${colorEntry(e.name, e.type)}`
    })
    return { type: 'html', text: `total ${entries.length * 8}<br>` + rows.join('<br>') }
  }

  // Vista en columnas (como ls real)
  const visuals = entries.map(e => ({
    entry: e,
    display: e.type === 'dir' ? e.name + '/' : e.name,
  }))
  const maxLen  = Math.max(...visuals.map(v => v.display.length))
  const colW    = maxLen + 2
  const numCols = Math.max(1, Math.min(visuals.length, Math.floor(80 / colW)))

  const rows = []
  for (let i = 0; i < visuals.length; i += numCols) {
    const row = visuals.slice(i, i + numCols)
    const cells = row.map((v, j) => {
      const pad = j < row.length - 1 ? ' '.repeat(colW - v.display.length) : ''
      return colorEntry(v.entry.name, v.entry.type) + pad
    })
    rows.push(cells.join(''))
  }
  return { type: 'html', text: rows.join('<br>') }
}

function cmdCd(args) {
  const raw    = args[0] ?? '/'
  const result = resolveCI(cwd.value, raw)
  if (!result)                    return { type: 'error', text: `cd: no such file or directory: ${raw}` }
  if (result.node.type !== 'dir') return { type: 'error', text: `cd: not a directory: ${raw}` }
  cwd.value = result.path
  return null
}

function cmdCat(args) {
  if (!args.length) return { type: 'error', text: 'cat: missing operand' }
  const results = []
  for (const arg of args) {
    const p    = resolvePath(cwd.value, arg)
    const node = getNode(p)
    if (!node)                return results.push({ type: 'error', text: `cat: ${arg}: No such file or directory` })
    if (node.type === 'dir')  return results.push({ type: 'error', text: `cat: ${arg}: Is a directory` })
    results.push({ type: 'plain', text: node.content || '' })
  }
  return results
}

function cmdGrep(args) {
  const flags = args.filter(a => a.startsWith('-'))
  const rest  = args.filter(a => !a.startsWith('-'))
  if (rest.length < 2) return { type: 'error', text: 'grep: uso: grep [patrón] [archivo]' }
  const [pattern, ...files] = rest
  const ignoreCase = flags.includes('-i')
  const results = []
  for (const file of files) {
    const p    = resolvePath(cwd.value, file)
    const node = getNode(p)
    if (!node || node.type === 'dir') { results.push({ type: 'error', text: `grep: ${file}: No such file or directory` }); continue }
    const lines = (node.content || '').split('\n')
    const regex = new RegExp(pattern, ignoreCase ? 'gi' : 'g')
    for (const line of lines) {
      if (regex.test(line)) {
        const hl = line.replace(new RegExp(pattern, ignoreCase ? 'gi' : 'g'),
          m => `<span style="color:#f85149;font-weight:bold">${m}</span>`)
        results.push({ type: 'html', text: files.length > 1 ? `<span style="color:#58a6ff">${file}:</span>${hl}` : hl })
      }
    }
  }
  return results.length ? results : null
}

function cmdMkdir(args) {
  if (!args.length) return { type: 'error', text: 'mkdir: missing operand' }
  const ts = new Date().toLocaleString('es', { month: 'short', day: '2-digit', hour: '2-digit', minute: '2-digit' })
  for (const arg of args) {
    const p = resolvePath(cwd.value, arg)
    const { parent, name } = getParentAndName(p)
    if (!parent)              return { type: 'error', text: `mkdir: cannot create directory '${arg}': No such file or directory` }
    if (parent.children[name]) return { type: 'error', text: `mkdir: cannot create directory '${arg}': File exists` }
    parent.children[name] = { type: 'dir', name, children: {}, modified: ts }
  }
  return null
}

function cmdTouch(args) {
  if (!args.length) return { type: 'error', text: 'touch: missing file operand' }
  const ts = new Date().toLocaleString('es', { month: 'short', day: '2-digit', hour: '2-digit', minute: '2-digit' })
  for (const arg of args) {
    const p = resolvePath(cwd.value, arg)
    const { parent, name } = getParentAndName(p)
    if (!parent) return { type: 'error', text: `touch: cannot touch '${arg}': No such file or directory` }
    if (!parent.children[name])
      parent.children[name] = { type: 'file', name, size: 0, content: '', modified: ts }
  }
  return null
}

function cmdRm(args) {
  const flags     = args.filter(a => a.startsWith('-'))
  const files     = args.filter(a => !a.startsWith('-'))
  const recursive = flags.some(f => f.includes('r') || f.includes('R'))
  if (!files.length) return { type: 'error', text: 'rm: missing operand' }
  for (const file of files) {
    const p = resolvePath(cwd.value, file)
    if (p === '/') return { type: 'error', text: 'rm: refusing to remove root directory' }
    const { parent, name } = getParentAndName(p)
    if (!parent || !parent.children[name]) return { type: 'error', text: `rm: cannot remove '${file}': No such file or directory` }
    const node = parent.children[name]
    if (node.type === 'dir' && !recursive) return { type: 'error', text: `rm: cannot remove '${file}': Is a directory (use -r)` }
    delete parent.children[name]
    // Si el cwd actual o un ancestro fue eliminado, volver a raíz
    if (cwd.value.startsWith(p)) cwd.value = '/'
  }
  return null
}

function cmdEcho(args)     { return { type: 'plain', text: args.join(' ') } }
function cmdPwd()          { return { type: 'plain', text: `/${vfsRoot.name}${cwd.value === '/' ? '' : cwd.value}` } }
function cmdDate()         { return { type: 'plain', text: new Date().toString() } }
function cmdWhoami()       { return { type: 'plain', text: username.value } }
function cmdUname(args) {
  if (args.includes('-a')) return { type: 'plain', text: `Linux ${HOSTNAME} 5.15.0-VirtualSO #1 SMP Mon Mar 15 10:00:00 UTC 2026 x86_64 GNU/Linux` }
  return { type: 'plain', text: 'Linux' }
}

function showHistory() {
  if (!cmdHistory.value.length) return { type: 'info', text: '(sin historial)' }
  return {
    type: 'html',
    text: cmdHistory.value.map((h, i) =>
      `<span style="color:#8b949e">${String(i + 1).padStart(4)}  </span>${h}`
    ).join('<br>'),
  }
}

function cmdTree(args) {
  const target = args[0] ? resolvePath(cwd.value, args[0]) : cwd.value
  const node   = getNode(target)
  if (!node || node.type !== 'dir') return { type: 'error', text: `tree: '${args[0] || '.'}': No such directory` }
  const lines = [colorEntry(node.name, 'dir')]
  function walk(dir, prefix) {
    const entries = Object.values(dir.children).sort((a, b) => {
      if (a.type !== b.type) return a.type === 'dir' ? -1 : 1
      return a.name.localeCompare(b.name)
    })
    entries.forEach((e, i) => {
      const isLast = i === entries.length - 1
      const branch = isLast ? '└── ' : '├── '
      const next   = isLast ? '    ' : '│   '
      lines.push(prefix + branch + colorEntry(e.name, e.type))
      if (e.type === 'dir') walk(e, prefix + next)
    })
  }
  walk(node, '')
  return { type: 'html', text: lines.join('<br>') }
}

function cmdHelp() {
  return {
    type: 'html',
    text:
      `<span style="color:#e3b341;font-weight:bold">MiniTerminal_SyJ — Comandos disponibles</span><br><br>` +
      [
        ['ls [-l] [-a]',      'Listar contenido del directorio'],
        ['cd [dir]',          'Cambiar directorio (~ para raíz)'],
        ['pwd',               'Mostrar ruta actual'],
        ['cat [archivo]',     'Mostrar contenido de un archivo'],
        ['echo [texto]',      'Imprimir texto'],
        ['grep [pat] [f]',    'Buscar patrón en archivo'],
        ['mkdir [nombre]',    'Crear directorio'],
        ['touch [nombre]',    'Crear archivo vacío'],
        ['rm [-r] [nombre]',  'Eliminar archivo o directorio'],
        ['tree [dir]',        'Ver árbol de directorios'],
        ['whoami',            'Mostrar usuario'],
        ['date',              'Fecha y hora actual'],
        ['uname [-a]',        'Información del sistema'],
        ['history',           'Historial de comandos'],
        ['clear',             'Limpiar pantalla'],
        ['help',              'Mostrar esta ayuda'],
        ['exit',              'Salir del shell'],
        ['cmd > archivo',     'Redirigir salida (Fase 4)'],
        ['cmd >> archivo',    'Agregar salida (Fase 4)'],
        ['cmd < archivo',     'Leer entrada desde archivo (Fase 4)'],
        ['cmd | cmd',         'Tubería (Fase 3)'],
        ['cmd &',             'Segundo plano (Fase 7)'],
      ].map(([cmd, desc]) =>
        `  <span style="color:#58a6ff">${cmd.padEnd(22)}</span><span style="color:#8b949e">${desc}</span>`
      ).join('<br>'),
  }
}

/* ─── Animación de procesos ───────────────────────────────────── */
function delay(ms) { return new Promise(r => setTimeout(r, ms)) }

async function runWithProcessEvents(cmdName, args, handler) {
  const childPid = ++pidCounter
  const shellPid = 1

  emit('process-event', { type: 'fork',    pid: childPid, ppid: shellPid, command: cmdName, args })
  await delay(350)
  emit('process-event', { type: 'exec',    pid: childPid, command: cmdName, args })
  await delay(280)
  emit('process-event', { type: 'wait',    pid: shellPid, waitFor: childPid })
  emit('process-event', { type: 'running', pid: childPid, command: cmdName })

  const result = handler()
  await delay(600)

  emit('process-event', { type: 'exit',   pid: childPid, exitCode: 0, command: cmdName })
  await delay(250)
  emit('process-event', { type: 'resume', pid: shellPid })
  return result
}

async function runInBackground(cmdName, args, handler) {
  const childPid = ++pidCounter
  const shellPid = 1

  emit('process-event', { type: 'fork',             pid: childPid, ppid: shellPid, command: cmdName, args, background: true })
  await delay(200)
  emit('process-event', { type: 'exec',             pid: childPid, command: cmdName, args })
  await delay(180)
  emit('process-event', { type: 'background-start', pid: childPid, shellPid })
  emit('process-event', { type: 'resume',           pid: shellPid })

  const result = handler()

  setTimeout(async () => {
    emit('process-event', { type: 'running', pid: childPid, command: cmdName })
    await delay(1400)
    emit('process-event', { type: 'exit',    pid: childPid, exitCode: 0, command: cmdName })
    outputLines.value.push({ type: 'info', text: `[1]+  Done    ${cmdName}` })
    await scrollBottom()
  }, 100)

  return { result, pid: childPid }
}

/* ─── Ejecución principal ─────────────────────────────────────── */
function pushLine(line) {
  if (Array.isArray(line)) line.forEach(l => l && outputLines.value.push(l))
  else if (line)            outputLines.value.push(line)
}

async function handleEnter() {
  const rawInput = currentInput.value.trim()
  currentInput.value = ''

  // Guardar snapshot del prompt en la línea de salida
  outputLines.value.push({ type: 'prompt', text: rawInput, path: promptLabel.value })
  if (!rawInput) { await scrollBottom(); return }

  if (cmdHistory.value[cmdHistory.value.length - 1] !== rawInput)
    cmdHistory.value.push(rawInput)
  histIdx.value = -1

  // Fase 7: segundo plano (&)
  const isBg = /\s*&\s*$/.test(rawInput)
  let cmdLine = rawInput.replace(/\s*&\s*$/, '').trim()

  // Normalizar variantes sin espacio: cd.., cd., cd~, cd/path → cd .., cd ., cd ~, cd /path
  cmdLine = cmdLine.replace(/^cd(\.{1,2}|~|\/\S*)/, (_, rest) => 'cd ' + rest)

  // Fase 3: pipe
  if (cmdLine.includes('|')) {
    await handlePipe(cmdLine)
    await scrollBottom()
    return
  }

  // Fase 4: redirecciones
  let outFile = null, appendMode = false, inFile = null
  const appendMatch = cmdLine.match(/^(.+?)\s*>>\s*(\S+)\s*$/)
  const outMatch    = cmdLine.match(/^(.+?)\s*(?<!>)>\s*(\S+)\s*$/)
  const inMatch     = cmdLine.match(/^(.+?)\s*<\s*(\S+)\s*$/)

  if (appendMatch)      { cmdLine = appendMatch[1].trim(); outFile = appendMatch[2]; appendMode = true }
  else if (outMatch)    { cmdLine = outMatch[1].trim();    outFile = outMatch[2] }
  if (inMatch)          { cmdLine = inMatch[1].trim();     inFile  = inMatch[2] }

  const tokens = cmdLine.split(/\s+/).filter(Boolean)
  const [rawCmd, ...args] = tokens
  const cmd = rawCmd?.toLowerCase()

  // Comandos que no generan fork/exec
  if (cmd === 'exit')  { pushLine({ type: 'info', text: 'Saliendo del Shell...' }); await scrollBottom(); return }
  if (cmd === 'clear') { outputLines.value = []; await scrollBottom(); return }

  if (inFile) {
    const node = getNode(resolvePath(cwd.value, inFile))
    if (!node || node.type !== 'file') {
      pushLine({ type: 'error', text: `bash: ${inFile}: No such file or directory` })
      pushLine({ type: 'blank' })
      await scrollBottom()
      return
    }
    args.push(inFile)
  }

  const handlers = {
    ls:      () => cmdLs(args),
    dir:     () => cmdLs(args),
    cd:      () => cmdCd(args),
    cat:     () => cmdCat(args),
    echo:    () => cmdEcho(args),
    grep:    () => cmdGrep(args),
    mkdir:   () => cmdMkdir(args),
    touch:   () => cmdTouch(args),
    rm:      () => cmdRm(args),
    tree:    () => cmdTree(args),
    pwd:     () => cmdPwd(),
    date:    () => cmdDate(),
    whoami:  () => cmdWhoami(),
    uname:   () => cmdUname(args),
    history: () => showHistory(),
    help:    () => cmdHelp(),
    man:     () => cmdHelp(),
  }

  const handler = handlers[cmd] || (() => ({ type: 'error', text: `bash: ${cmd}: command not found` }))

  processing.value = !isBg
  let result = null

  if (isBg) {
    const { result: bgResult, pid } = await runInBackground(cmd, args, handler)
    pushLine({ type: 'info', text: `[1] ${pid}` })
    result = outFile ? null : bgResult
  } else {
    result = await runWithProcessEvents(cmd, args, handler)
  }

  if (outFile && result !== null) {
    const text = extractText(result)
    const ok   = writeToFile(outFile, text, appendMode)
    if (!ok) pushLine({ type: 'error', text: `bash: ${outFile}: Permission denied or is a directory` })
    else     pushLine({ type: 'info',  text: `→ Salida redirigida a ${outFile}` })
    result = null
  }

  pushLine(result)
  pushLine({ type: 'blank' })
  processing.value = false
  await scrollBottom()
  await nextTick()
  inputRef.value?.focus()
}

async function handlePipe(input) {
  const parts    = input.split('|').map(s => s.trim())
  const shellPid = 1
  const pid1     = ++pidCounter
  const pid2     = ++pidCounter
  const [cmd1tokens, cmd2tokens] = parts.map(p => p.split(/\s+/).filter(Boolean))

  emit('process-event', { type: 'fork', pid: pid1, ppid: shellPid, command: cmd1tokens[0], pipe: true })
  await delay(200)
  emit('process-event', { type: 'fork', pid: pid2, ppid: shellPid, command: cmd2tokens[0], pipe: true, pipeFrom: pid1 })
  await delay(200)
  emit('process-event', { type: 'exec', pid: pid1, command: cmd1tokens[0] })
  await delay(150)
  emit('process-event', { type: 'exec', pid: pid2, command: cmd2tokens[0] })
  await delay(200)
  emit('process-event', { type: 'wait', pid: shellPid, waitFor: pid1 })
  emit('process-event', { type: 'pipe-running', pid1, pid2 })
  await delay(700)
  emit('process-event', { type: 'exit', pid: pid1, exitCode: 0, command: cmd1tokens[0] })
  await delay(300)
  emit('process-event', { type: 'exit', pid: pid2, exitCode: 0, command: cmd2tokens[0] })
  await delay(200)
  emit('process-event', { type: 'resume', pid: shellPid })

  const src = { ls: () => cmdLs(cmd1tokens.slice(1)), cat: () => cmdCat(cmd1tokens.slice(1)) }
  let intermediate = src[cmd1tokens[0]]?.() ?? null
  let finalResult  = null

  if (intermediate && cmd2tokens[0] === 'grep' && cmd2tokens[1]) {
    const pattern = cmd2tokens[1]
    const lines   = (intermediate.text || '').replace(/<[^>]+>/g, '').split(/\n|<br>/)
    const matched = lines.filter(l => l.toLowerCase().includes(pattern.toLowerCase()))
    finalResult = matched.length
      ? { type: 'html', text: matched.map(l =>
          l.replace(new RegExp(pattern, 'gi'), m => `<span style="color:#f85149;font-weight:bold">${m}</span>`)
        ).join('<br>') }
      : null
  } else {
    finalResult = intermediate
  }

  pushLine(finalResult)
  pushLine({ type: 'blank' })
}

/* ─── Historial de navegación ─────────────────────────────────── */
function historyUp() {
  if (!cmdHistory.value.length) return
  histIdx.value = Math.min(histIdx.value + 1, cmdHistory.value.length - 1)
  currentInput.value = cmdHistory.value[cmdHistory.value.length - 1 - histIdx.value]
}

function historyDown() {
  if (histIdx.value <= 0) { histIdx.value = -1; currentInput.value = ''; return }
  histIdx.value--
  currentInput.value = cmdHistory.value[cmdHistory.value.length - 1 - histIdx.value]
}

/* ─── Tab completion ──────────────────────────────────────────── */
function handleTab() {
  const tokens = currentInput.value.split(/\s+/)
  const last   = tokens[tokens.length - 1]
  if (!last) return
  const node = getNode(cwd.value)
  if (!node || node.type !== 'dir') return
  const matches = Object.keys(node.children).filter(k => k.startsWith(last))
  if (matches.length === 1) {
    tokens[tokens.length - 1] = matches[0] + (node.children[matches[0]].type === 'dir' ? '/' : '')
    currentInput.value = tokens.join(' ')
  }
}

/* ─── Scroll / focus ──────────────────────────────────────────── */
async function scrollBottom() {
  await nextTick()
  if (outputRef.value) outputRef.value.scrollTop = outputRef.value.scrollHeight
}

function focusInput() { inputRef.value?.focus() }

/* ─── Mensaje de bienvenida ───────────────────────────────────── */
onMounted(() => {
  outputLines.value.push(
    { type: 'html',  text: `<span style="color:#58a6ff;font-weight:bold">MiniTerminal_SyJ v1.0</span> — <span style="color:#8b949e">Sistemas Operativos, Universidad Rafael Landívar</span>` },
    { type: 'html',  text: `<span style="color:#8b949e">Carga una carpeta en el <span style="color:#f0883e">Explorador de Archivos</span> para empezar. Luego usa <span style="color:#7ee787">ls</span>, <span style="color:#7ee787">cd</span>, <span style="color:#7ee787">cat</span>, etc.</span>` },
    { type: 'html',  text: `<span style="color:#8b949e">Escribe <span style="color:#7ee787">help</span> para ver todos los comandos.</span>` },
    { type: 'blank' },
  )
  nextTick(() => inputRef.value?.focus())
})
</script>

<style scoped>
.terminal {
  flex: 1;
  display: flex;
  flex-direction: column;
  background: #0d1117;
  overflow: hidden;
  cursor: text;
  font-family: 'Courier New', Courier, monospace;
  font-size: 0.82rem;
  line-height: 1.5;
}

.output {
  flex: 1;
  overflow-y: auto;
  padding: 10px 14px;
  word-break: break-all;
}

.line-prompt  { display: block; }
.line-html    { display: block; }
.line-error   { display: block; color: #f85149; }
.line-info    { display: block; color: #8b949e; font-style: italic; }
.line-blank   { display: block; height: 0.4em; }
.line-plain   { display: block; color: #c9d1d9; white-space: pre-wrap; }

.prompt-path { color: #56d364; font-weight: bold; }
.prompt-cmd  { color: #e6edf3; }

.input-row {
  display: flex;
  align-items: center;
  padding: 8px 14px;
  border-top: 1px solid #21262d;
  background: #0d1117;
  flex-shrink: 0;
}

.terminal-input {
  flex: 1;
  background: transparent;
  border: none;
  outline: none;
  color: #e6edf3;
  font-family: inherit;
  font-size: inherit;
  caret-color: #56d364;
}

.processing-row  { color: #8b949e; }
.processing-dots .dot1,
.processing-dots .dot2,
.processing-dots .dot3 { animation: blink 1.2s infinite; }
.processing-dots .dot2 { animation-delay: 0.4s; }
.processing-dots .dot3 { animation-delay: 0.8s; }

@keyframes blink {
  0%, 100% { opacity: 0.2; }
  50%       { opacity: 1; }
}
</style>
