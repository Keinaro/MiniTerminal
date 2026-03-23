<template>
  <div class="terminal" @click="focusInput">
    <!-- Área de salida -->
    <div class="output" ref="outputRef">
      <div v-for="(line, i) in outputLines" :key="i">
        <span v-if="line.type === 'prompt'" class="line-prompt">
          <span class="prompt-text">MiniTerminal_SyJ&gt;&nbsp;</span>
          <span class="prompt-cmd">{{ line.text }}</span>
        </span>
        <span v-else-if="line.type === 'html'" v-html="line.text" class="line-html"></span>
        <span v-else-if="line.type === 'error'" class="line-error">{{ line.text }}</span>
        <span v-else-if="line.type === 'info'" class="line-info">{{ line.text }}</span>
        <span v-else-if="line.type === 'blank'" class="line-blank">&nbsp;</span>
        <span v-else class="line-plain">{{ line.text }}</span>
      </div>
    </div>

    <!-- Fila de entrada activa -->
    <div class="input-row" v-if="!processing">
      <span class="prompt-text">MiniTerminal_SyJ&gt;&nbsp;</span>
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
import { ref, nextTick, onMounted } from 'vue'

const emit = defineEmits(['process-event'])

/* ─── VFS (Virtual Filesystem) ───────────────────────────── */
const vfs = {
  '/': {
    type: 'dir', name: '/',
    children: {
      home: {
        type: 'dir', name: 'home',
        children: {
          usuario: {
            type: 'dir', name: 'usuario',
            children: {
              'terminal.cpp': {
                type: 'file', name: 'terminal.cpp', size: 2847,
                modified: 'Mar 15 10:23',
                content: `#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <cstring>
#ifdef _WIN32
#include <process.h>
#else
#include <unistd.h>
#include <sys/wait.h>
#endif

void ejecutar_comando(const std::vector<std::string>& args_str) {
    if (args_str.empty()) return;
    char** args = new char*[args_str.size() + 1];
    for (size_t i = 0; i < args_str.size(); ++i) {
        args[i] = new char[args_str[i].size() + 1];
        std::strcpy(args[i], args_str[i].c_str());
    }
    args[args_str.size()] = nullptr;
#ifndef _WIN32
    pid_t pid = fork();
    if (pid == 0) {
        execvp(args[0], args);
        _exit(EXIT_FAILURE);
    } else {
        waitpid(pid, nullptr, 0);
    }
#endif
    for (size_t i = 0; i < args_str.size(); ++i) delete[] args[i];
    delete[] args;
}

int main() {
    std::string linea;
    while (true) {
        std::cout << "MiniTerminal_SyJ> ";
        if (!std::getline(std::cin, linea)) break;
        std::vector<std::string> args_str;
        std::istringstream iss(linea);
        std::string token;
        while (iss >> token) args_str.push_back(token);
        if (!args_str.empty() && args_str[0] == "exit") break;
        ejecutar_comando(args_str);
    }
    return 0;
}`
              },
              'README.md': {
                type: 'file', name: 'README.md', size: 412,
                modified: 'Mar 15 09:10',
                content: `# MiniTerminal — Sistemas Operativos

Implementación de una mini shell en C++ para la clase de Sistemas Operativos.

## Compilar

\`\`\`bash
g++ -std=c++17 -Wall -o mishell terminal.cpp
\`\`\`

## Ejecutar

\`\`\`bash
./mishell
\`\`\`

## Comandos soportados

- Cualquier ejecutable del sistema (ls, date, echo, etc.)
- exit — salir del shell
`
              },
              proyectos: {
                type: 'dir', name: 'proyectos', modified: 'Mar 20 14:00',
                children: {
                  'practica1.txt': {
                    type: 'file', name: 'practica1.txt', size: 256,
                    modified: 'Mar 20 14:00',
                    content: 'Practica 1: Llamadas al sistema\nfork(), exec(), wait(), exit()\n\nImplementar un shell basico con soporte de tuberias.'
                  },
                  'notas_procesos.txt': {
                    type: 'file', name: 'notas_procesos.txt', size: 189,
                    modified: 'Mar 18 11:30',
                    content: 'Ciclo de vida de un proceso:\n1. fork() -> crea proceso hijo\n2. exec() -> reemplaza imagen\n3. wait() -> padre espera\n4. exit() -> proceso termina'
                  }
                }
              }
            }
          }
        }
      },
      tmp:  { type: 'dir', name: 'tmp',  children: {}, modified: 'Mar 01 00:00' },
      etc:  { type: 'dir', name: 'etc',  modified: 'Jan 01 00:00',
        children: {
          'os-release': {
            type: 'file', name: 'os-release', size: 120, modified: 'Jan 01 00:00',
            content: 'NAME="MiniTerminal OS"\nVERSION="1.0"\nID=miniterminal\nPRETTY_NAME="MiniTerminal OS 1.0"'
          }
        }
      },
      bin:  { type: 'dir', name: 'bin',  children: {}, modified: 'Jan 01 00:00' },
      var:  { type: 'dir', name: 'var',  children: {}, modified: 'Jan 01 00:00' }
    }
  }
}

/* ─── Estado de la terminal ───────────────────────────────── */
const HOME = '/home/usuario'
const outputLines = ref([])
const currentInput = ref('')
const inputRef = ref(null)
const outputRef = ref(null)
const cwd = ref(HOME)
const history = ref([])
const histIdx = ref(-1)
const processing = ref(false)
let pidCounter = 100

/* ─── Helpers VFS ─────────────────────────────────────────── */
function resolvePath(base, target) {
  if (!target || target === '.') return base
  if (target === '~') return HOME
  if (target === '..') {
    const parts = base.split('/').filter(Boolean)
    parts.pop()
    return '/' + parts.join('/')
  }
  if (target.startsWith('~/')) return HOME + '/' + target.slice(2)
  if (target.startsWith('/'))  return target
  if (target.startsWith('../')) {
    const parts = base.split('/').filter(Boolean)
    parts.pop()
    return resolvePath('/' + parts.join('/'), target.slice(3))
  }
  return base === '/' ? '/' + target : base + '/' + target
}

function getNode(path) {
  if (path === '/') return vfs['/']
  const parts = path.split('/').filter(Boolean)
  let node = vfs['/']
  for (const part of parts) {
    if (!node || node.type !== 'dir' || !node.children[part]) return null
    node = node.children[part]
  }
  return node
}

function getParentAndName(path) {
  const parts = path.split('/').filter(Boolean)
  const name = parts.pop()
  const parentPath = '/' + parts.join('/')
  return { parent: getNode(parentPath), name, parentPath }
}

/* ─── Colorización de entradas ───────────────────────────── */
const EXT_COLORS = {
  cpp: '#7ee787', c: '#7ee787', h: '#7ee787',
  js: '#e3b341', ts: '#58a6ff', vue: '#56d364',
  md: '#e6edf3', txt: '#e6edf3', pdf: '#f85149',
  sh: '#ffa657', py: '#ffa657',
  json: '#79c0ff', yaml: '#79c0ff', yml: '#79c0ff'
}

function colorEntry(name, type) {
  if (type === 'dir') return `<span style="color:#58a6ff;font-weight:bold">${name}/</span>`
  const ext = name.split('.').pop().toLowerCase()
  const color = EXT_COLORS[ext] || '#c9d1d9'
  return `<span style="color:${color}">${name}</span>`
}

/* ─── Comandos ────────────────────────────────────────────── */
const USERNAME = 'usuario'
const HOSTNAME = 'miniterminal'

function cmdLs(args) {
  const flags = args.filter(a => a.startsWith('-'))
  const paths = args.filter(a => !a.startsWith('-'))
  const longFmt = flags.some(f => f.includes('l'))
  const showHidden = flags.some(f => f.includes('a'))
  const target = paths[0] ? resolvePath(cwd.value, paths[0]) : cwd.value
  const node = getNode(target)
  if (!node) return { type: 'error', text: `ls: cannot access '${paths[0]}': No such file or directory` }
  if (node.type === 'file') return { type: 'html', text: colorEntry(node.name, 'file') }

  let entries = Object.values(node.children)
  if (!showHidden) entries = entries.filter(e => !e.name.startsWith('.'))
  entries.sort((a, b) => {
    if (a.type !== b.type) return a.type === 'dir' ? -1 : 1
    return a.name.localeCompare(b.name)
  })

  if (!entries.length) return null

  if (longFmt) {
    const rows = entries.map(e => {
      const perm = e.type === 'dir' ? 'drwxr-xr-x' : '-rw-r--r--'
      const sz = String(e.size || (e.type === 'dir' ? 4096 : (e.content?.length || 0))).padStart(6)
      const mod = e.modified || 'Jan  1 00:00'
      return `${perm}  1 ${USERNAME} ${USERNAME} ${sz} ${mod} ${colorEntry(e.name, e.type)}`
    })
    return { type: 'html', text: `total ${entries.length * 8}<br>` + rows.join('<br>') }
  }

  const cols = entries.map(e => colorEntry(e.name, e.type))
  return { type: 'html', text: cols.join('&nbsp;&nbsp;&nbsp;') }
}

function cmdCd(args) {
  const target = args[0] ? resolvePath(cwd.value, args[0]) : HOME
  const node = getNode(target)
  if (!node)             return { type: 'error', text: `cd: no such file or directory: ${args[0]}` }
  if (node.type !== 'dir') return { type: 'error', text: `cd: not a directory: ${args[0]}` }
  cwd.value = target
  return null
}

function cmdCat(args) {
  if (!args.length) return { type: 'error', text: 'cat: missing operand' }
  const results = []
  for (const arg of args) {
    const p = resolvePath(cwd.value, arg)
    const node = getNode(p)
    if (!node) { results.push({ type: 'error', text: `cat: ${arg}: No such file or directory` }); continue }
    if (node.type === 'dir') { results.push({ type: 'error', text: `cat: ${arg}: Is a directory` }); continue }
    results.push({ type: 'plain', text: node.content || '' })
  }
  return results
}

function cmdGrep(args) {
  const flags = args.filter(a => a.startsWith('-'))
  const rest = args.filter(a => !a.startsWith('-'))
  if (rest.length < 2) return { type: 'error', text: 'grep: uso: grep [patrón] [archivo]' }
  const [pattern, ...files] = rest
  const ignoreCase = flags.includes('-i')
  const results = []
  for (const file of files) {
    const p = resolvePath(cwd.value, file)
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
  for (const arg of args) {
    const p = resolvePath(cwd.value, arg)
    const { parent, name } = getParentAndName(p)
    if (!parent) return { type: 'error', text: `mkdir: cannot create directory '${arg}': No such file or directory` }
    if (parent.children[name]) return { type: 'error', text: `mkdir: cannot create directory '${arg}': File exists` }
    parent.children[name] = { type: 'dir', name, children: {}, modified: new Date().toLocaleString('es', { month: 'short', day: '2-digit', hour: '2-digit', minute: '2-digit' }) }
  }
  return null
}

function cmdTouch(args) {
  if (!args.length) return { type: 'error', text: 'touch: missing file operand' }
  for (const arg of args) {
    const p = resolvePath(cwd.value, arg)
    const { parent, name } = getParentAndName(p)
    if (!parent) return { type: 'error', text: `touch: cannot touch '${arg}': No such file or directory` }
    if (!parent.children[name]) {
      parent.children[name] = { type: 'file', name, size: 0, content: '', modified: new Date().toLocaleString('es', { month: 'short', day: '2-digit', hour: '2-digit', minute: '2-digit' }) }
    }
  }
  return null
}

function cmdRm(args) {
  const flags = args.filter(a => a.startsWith('-'))
  const files = args.filter(a => !a.startsWith('-'))
  const recursive = flags.some(f => f.includes('r') || f.includes('R'))
  if (!files.length) return { type: 'error', text: 'rm: missing operand' }
  for (const file of files) {
    const p = resolvePath(cwd.value, file)
    if (p === '/' || p === HOME) return { type: 'error', text: 'rm: refusing to remove root or home directory' }
    const { parent, name } = getParentAndName(p)
    if (!parent || !parent.children[name]) return { type: 'error', text: `rm: cannot remove '${file}': No such file or directory` }
    const node = parent.children[name]
    if (node.type === 'dir' && !recursive) return { type: 'error', text: `rm: cannot remove '${file}': Is a directory (use -r)` }
    delete parent.children[name]
  }
  return null
}

function cmdEcho(args) {
  return { type: 'plain', text: args.join(' ') }
}

function cmdPwd() {
  return { type: 'plain', text: cwd.value }
}

function cmdDate() {
  return { type: 'plain', text: new Date().toString() }
}

function cmdWhoami() {
  return { type: 'plain', text: USERNAME }
}

function cmdUname(args) {
  if (args.includes('-a')) return { type: 'plain', text: `Linux ${HOSTNAME} 5.15.0-VirtualSO #1 SMP Mon Mar 15 10:00:00 UTC 2026 x86_64 GNU/Linux` }
  return { type: 'plain', text: 'Linux' }
}

function cmdHistory() {
  if (!history.value.length) return { type: 'info', text: '(sin historial)' }
  return {
    type: 'html',
    text: history.value.map((h, i) =>
      `<span style="color:#8b949e">${String(i + 1).padStart(4)}  </span>${h}`
    ).join('<br>')
  }
}

function cmdHelp() {
  return {
    type: 'html',
    text: `<span style="color:#e3b341;font-weight:bold">MiniTerminal_SyJ — Comandos disponibles</span><br><br>` +
    [
      ['ls [-l] [-a]',      'Listar contenido del directorio'],
      ['cd [dir]',          'Cambiar directorio (~ para home)'],
      ['pwd',               'Mostrar directorio actual'],
      ['cat [archivo]',     'Mostrar contenido de archivo'],
      ['echo [texto]',      'Imprimir texto'],
      ['grep [patrón] [f]', 'Buscar patrón en archivo'],
      ['mkdir [nombre]',    'Crear directorio'],
      ['touch [nombre]',    'Crear archivo vacío'],
      ['rm [-r] [nombre]',  'Eliminar archivo/directorio'],
      ['whoami',            'Mostrar usuario actual'],
      ['date',              'Mostrar fecha y hora'],
      ['uname [-a]',        'Información del sistema'],
      ['history',           'Historial de comandos'],
      ['clear',             'Limpiar pantalla'],
      ['help',              'Mostrar esta ayuda'],
      ['exit',              'Salir del shell'],
    ].map(([cmd, desc]) =>
      `  <span style="color:#58a6ff">${cmd.padEnd(22)}</span><span style="color:#8b949e">${desc}</span>`
    ).join('<br>')
  }
}

/* ─── Ejecución con eventos de proceso ───────────────────── */
function delay(ms) {
  return new Promise(resolve => setTimeout(resolve, ms))
}

async function runWithProcessEvents(cmdName, args, handler) {
  const childPid = ++pidCounter
  const shellPid = 1

  // 1. fork()
  emit('process-event', { type: 'fork', pid: childPid, ppid: shellPid, command: cmdName, args })
  await delay(350)

  // 2. exec()
  emit('process-event', { type: 'exec', pid: childPid, command: cmdName, args })
  await delay(280)

  // 3. wait() — padre espera
  emit('process-event', { type: 'wait', pid: shellPid, waitFor: childPid })
  emit('process-event', { type: 'running', pid: childPid, command: cmdName })

  // 4. Ejecutar el comando real
  const result = handler()
  await delay(600)

  // 5. exit()
  emit('process-event', { type: 'exit', pid: childPid, exitCode: 0, command: cmdName })
  await delay(250)

  // 6. resume — padre continúa
  emit('process-event', { type: 'resume', pid: shellPid })

  return result
}

/* ─── Manejo del input ────────────────────────────────────── */
function pushLine(line) {
  if (Array.isArray(line)) {
    line.forEach(l => l && outputLines.value.push(l))
  } else if (line) {
    outputLines.value.push(line)
  }
}

async function handleEnter() {
  const input = currentInput.value.trim()
  currentInput.value = ''

  // Mostrar la línea del prompt con el comando
  outputLines.value.push({ type: 'prompt', text: input })

  if (!input) { await scrollBottom(); return }

  // Guardar en historial
  if (history.value[history.value.length - 1] !== input) {
    history.value.push(input)
  }
  histIdx.value = -1

  // Parsear
  const tokens = input.split(/\s+/).filter(Boolean)
  const [cmd, ...args] = tokens

  // Detectar pipes simples (cmd1 | cmd2)
  if (input.includes('|')) {
    await handlePipe(input)
    await scrollBottom()
    return
  }

  // Comandos internos (no generan fork/exec)
  if (cmd === 'exit') {
    pushLine({ type: 'info', text: 'Saliendo del Shell...' })
    await scrollBottom()
    return
  }
  if (cmd === 'clear') {
    outputLines.value = []
    await scrollBottom()
    return
  }

  // Comandos que generan proceso hijo
  processing.value = true
  let result = null

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
    pwd:     () => cmdPwd(),
    date:    () => cmdDate(),
    whoami:  () => cmdWhoami(),
    uname:   () => cmdUname(args),
    history: () => cmdHistory(),
    help:    () => cmdHelp(),
    man:     () => cmdHelp(),
  }

  if (handlers[cmd]) {
    result = await runWithProcessEvents(cmd, args, handlers[cmd])
  } else {
    // Simular comando no encontrado también pasa por fork/exec
    result = await runWithProcessEvents(cmd, args, () => ({
      type: 'error',
      text: `bash: ${cmd}: command not found`
    }))
  }

  pushLine(result)
  pushLine({ type: 'blank' })
  processing.value = false
  await scrollBottom()
  await nextTick()
  inputRef.value?.focus()
}

async function handlePipe(input) {
  const parts = input.split('|').map(s => s.trim())
  const shellPid = 1

  // Simular dos procesos con pipe
  const pid1 = ++pidCounter
  const pid2 = ++pidCounter
  const [cmd1tokens, cmd2tokens] = parts.map(p => p.split(/\s+/).filter(Boolean))

  emit('process-event', { type: 'fork', pid: pid1, ppid: shellPid, command: cmd1tokens[0], args: cmd1tokens.slice(1), pipe: true })
  await delay(200)
  emit('process-event', { type: 'fork', pid: pid2, ppid: shellPid, command: cmd2tokens[0], args: cmd2tokens.slice(1), pipe: true, pipeFrom: pid1 })
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

  // Ejecutar pipe real (simulado)
  const handlers = { ls: () => cmdLs(cmd1tokens.slice(1)), cat: () => cmdCat(cmd1tokens.slice(1)) }
  let intermediate = handlers[cmd1tokens[0]] ? handlers[cmd1tokens[0]]() : null
  let finalResult = null
  if (intermediate && cmd2tokens[0] === 'grep') {
    const pattern = cmd2tokens[1]
    if (pattern && intermediate.text) {
      const lines = intermediate.text.replace(/<[^>]+>/g, '').split(/\n|<br>/)
      const matched = lines.filter(l => l.toLowerCase().includes(pattern.toLowerCase()))
      finalResult = matched.length
        ? { type: 'html', text: matched.map(l => l.replace(new RegExp(pattern, 'gi'), m => `<span style="color:#f85149;font-weight:bold">${m}</span>`)).join('<br>') }
        : null
    }
  } else {
    finalResult = intermediate
  }

  pushLine(finalResult)
  pushLine({ type: 'blank' })
}

/* ─── Historial de navegación ─────────────────────────────── */
function historyUp() {
  if (!history.value.length) return
  histIdx.value = Math.min(histIdx.value + 1, history.value.length - 1)
  currentInput.value = history.value[history.value.length - 1 - histIdx.value]
}

function historyDown() {
  if (histIdx.value <= 0) { histIdx.value = -1; currentInput.value = ''; return }
  histIdx.value--
  currentInput.value = history.value[history.value.length - 1 - histIdx.value]
}

/* ─── Tab completion ──────────────────────────────────────── */
function handleTab() {
  const tokens = currentInput.value.split(/\s+/)
  const last = tokens[tokens.length - 1]
  if (!last) return
  const node = getNode(cwd.value)
  if (!node || node.type !== 'dir') return
  const matches = Object.keys(node.children).filter(k => k.startsWith(last))
  if (matches.length === 1) {
    tokens[tokens.length - 1] = matches[0] + (node.children[matches[0]].type === 'dir' ? '/' : '')
    currentInput.value = tokens.join(' ')
  }
}

/* ─── Scroll y focus ──────────────────────────────────────── */
async function scrollBottom() {
  await nextTick()
  if (outputRef.value) outputRef.value.scrollTop = outputRef.value.scrollHeight
}

function focusInput() {
  inputRef.value?.focus()
}

/* ─── Mensaje de bienvenida ───────────────────────────────── */
onMounted(() => {
  outputLines.value.push(
    { type: 'html', text: `<span style="color:#58a6ff;font-weight:bold">MiniTerminal_SyJ v1.0</span> — <span style="color:#8b949e">Sistemas Operativos, Universidad Rafael Landívar</span>` },
    { type: 'html', text: `<span style="color:#8b949e">Escribe <span style="color:#7ee787">help</span> para ver los comandos disponibles.</span>` },
    { type: 'blank' }
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
  padding: 0;
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

.line-prompt   { display: block; }
.line-html     { display: block; }
.line-error    { display: block; color: #f85149; }
.line-info     { display: block; color: #8b949e; font-style: italic; }
.line-blank    { display: block; height: 0.4em; }
.line-plain    { display: block; color: #c9d1d9; white-space: pre-wrap; }

.prompt-text { color: #56d364; font-weight: bold; }
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

.processing-row { color: #8b949e; }

.processing-dots .dot1,
.processing-dots .dot2,
.processing-dots .dot3 {
  animation: blink 1.2s infinite;
}
.processing-dots .dot2 { animation-delay: 0.4s; }
.processing-dots .dot3 { animation-delay: 0.8s; }

@keyframes blink {
  0%, 100% { opacity: 0.2; }
  50%       { opacity: 1; }
}
</style>
