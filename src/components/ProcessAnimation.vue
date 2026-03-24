<template>
  <div class="proc-anim">

    <!-- ── Escenario de procesos ───────────────── -->
    <div class="stage">
      <div class="stage-label">🏢 Área de Trabajo del SO</div>

      <!-- Proceso Shell (siempre visible) -->
      <div class="proc-card manager" :class="shell.state">
        <div class="card-avatar">{{ shellEmoji }}</div>
        <div class="card-body">
          <div class="card-name">Shell (Manager)</div>
          <div class="card-pid">PID: 1</div>
          <div class="state-badge" :class="shell.state">{{ stateLabel(shell.state) }}</div>
        </div>
        <div v-if="shell.state === 'waiting'" class="zzz-wrap">
          <span class="zzz">z</span><span class="zzz d1">z</span><span class="zzz d2">Z</span>
        </div>
        <div v-else-if="shell.state === 'running'" class="pulse-ring"></div>
      </div>

      <!-- Líneas de fork + tarjetas hijos -->
      <TransitionGroup name="child-anim" tag="div" class="children-area">
        <div v-for="child in children" :key="child.pid" class="child-slot">
          <!-- Conector fork -->
          <div class="fork-line">
            <div class="fork-label">
              <span v-if="child.pipeFrom">pipe()</span>
              <span v-else>fork() → exec()</span>
            </div>
          </div>

          <!-- Tarjeta del proceso hijo -->
          <div class="proc-card worker" :class="child.state">
            <div class="card-avatar">{{ childEmoji(child) }}</div>
            <div class="card-body">
              <div class="card-name">{{ child.command }}</div>
              <div class="card-pid">PID: {{ child.pid }}</div>
              <div class="state-badge" :class="child.state">{{ stateLabel(child.state) }}</div>
            </div>

            <div v-if="child.state === 'running'" class="progress-wrap">
              <div class="progress-bar">
                <div class="progress-fill" :style="{ width: child.progress + '%' }"></div>
              </div>
              <span class="progress-pct">{{ child.progress }}%</span>
            </div>

            <!-- Indicador de pipe entre hijos -->
            <div v-if="child.pipeFrom" class="pipe-tag">📤 PIPE ←</div>
            <!-- Indicador de proceso en segundo plano -->
            <div v-if="child.isBackground" class="bg-tag">🌙 BG</div>
          </div>
        </div>
      </TransitionGroup>
    </div>

    <!-- ── Indicador de pasos del ciclo de vida ── -->
    <div class="lifecycle">
      <div
        v-for="step in STEPS"
        :key="step.key"
        class="step"
        :class="{
          active: currentStep === step.key,
          done:   doneSteps.has(step.key)
        }"
      >
        <div class="step-icon">{{ step.icon }}</div>
        <div class="step-key">{{ step.key }}()</div>
        <div class="step-desc">{{ step.desc }}</div>
      </div>
    </div>

    <!-- ── Log de syscalls ────────────────────── -->
    <div class="syscall-log">
      <div class="log-head">📋 Registro de Llamadas al Sistema</div>
      <div class="log-body" ref="logRef">
        <TransitionGroup name="log-entry">
          <div v-for="entry in log" :key="entry.id" class="log-line" :class="entry.cls">
            <span class="log-time">{{ entry.time }}</span>
            <span class="log-icon">{{ entry.icon }}</span>
            <span class="log-text" v-html="entry.text"></span>
          </div>
        </TransitionGroup>
        <div v-if="!log.length" class="log-empty">
          Esperando comandos... Escribe algo en la terminal →
        </div>
      </div>
    </div>

    <!-- ── Leyenda ────────────────────────────── -->
    <div class="legend">
      <span v-for="item in LEGEND" :key="item.label" class="leg-item">
        <span class="leg-dot" :style="{ background: item.color }"></span>
        <span class="leg-label">{{ item.label }}</span>
      </span>
    </div>

  </div>
</template>

<script setup>
import { ref, reactive, watch, nextTick, computed } from 'vue'

const props = defineProps({ events: Array })

/* ─── Constantes ────────────────────────────────────────── */
const STEPS = [
  { key: 'fork',    icon: '🔀', desc: 'Crear hijo'   },
  { key: 'exec',    icon: '🔄', desc: 'Nueva imagen' },
  { key: 'wait',    icon: '⏳', desc: 'Padre espera' },
  { key: 'running', icon: '🏃', desc: 'Ejecutando'   },
  { key: 'exit',    icon: '✅', desc: 'Terminar'     },
]

const LEGEND = [
  { label: 'Inactivo',   color: '#30363d' },
  { label: 'fork()',     color: '#ffa657' },
  { label: 'exec()',     color: '#58a6ff' },
  { label: 'Ejecutando', color: '#56d364' },
  { label: 'wait()',     color: '#e3b341' },
  { label: 'exit()',     color: '#f85149' },
]

const CMD_EMOJI = {
  ls: '📂', dir: '📂', cat: '📄', echo: '📢', grep: '🔍',
  mkdir: '📁', touch: '✏️', rm: '🗑️', pwd: '📍', date: '📅',
  whoami: '👤', uname: '🖥️', history: '📜', help: '❓', man: '📖',
  default: '⚙️'
}

/* ─── Estado reactivo ──────────────────────────────────── */
const shell = reactive({ state: 'idle' })
const children = reactive([])
const log = ref([])
const logRef = ref(null)
const currentStep = ref('')
const doneSteps = reactive(new Set())

let logId = 0

/* ─── Helpers ──────────────────────────────────────────── */
const shellEmoji = computed(() => {
  const map = { idle: '👨‍💼', running: '💻', waiting: '⏳' }
  return map[shell.state] || '👨‍💼'
})

function childEmoji(child) {
  const stateMap = { created: '🥚', execing: '🔄', running: '🏃', exited: '✅' }
  if (child.state === 'running') return CMD_EMOJI[child.command] || CMD_EMOJI.default
  return stateMap[child.state] || CMD_EMOJI[child.command] || CMD_EMOJI.default
}

function stateLabel(state) {
  const labels = {
    idle:    'INACTIVO',
    running: 'ACTIVO',
    waiting: 'WAIT()',
    created: 'FORK()',
    execing: 'EXEC()',
    exited:  'EXIT()',
  }
  return labels[state] || state.toUpperCase()
}

function now() {
  return new Date().toLocaleTimeString('es', { hour: '2-digit', minute: '2-digit', second: '2-digit' })
}

function addLog(cls, icon, text) {
  log.value.push({ id: ++logId, time: now(), cls, icon, text })
  if (log.value.length > 60) log.value.shift()
  nextTick(() => {
    if (logRef.value) logRef.value.scrollTop = logRef.value.scrollHeight
  })
}

function animateProgress(child) {
  child.progress = 0
  const interval = setInterval(() => {
    if (child.progress >= 100 || child.state === 'exited') {
      clearInterval(interval)
      return
    }
    child.progress = Math.min(child.progress + Math.random() * 18 + 6, 98)
  }, 120)
}

/* ─── Manejo de eventos ────────────────────────────────── */
watch(
  () => props.events?.length,
  () => {
    if (!props.events?.length) return
    handleEvent(props.events[props.events.length - 1])
  }
)

function handleEvent(ev) {
  if (!ev) return

  switch (ev.type) {

    case 'fork': {
      shell.state = 'running'
      currentStep.value = 'fork'
      doneSteps.delete('exec'); doneSteps.delete('wait'); doneSteps.delete('running'); doneSteps.delete('exit')
      children.push({
        pid: ev.pid, ppid: ev.ppid,
        command: ev.command || '...',
        state: 'created', progress: 0,
        pipeFrom: ev.pipeFrom || null
      })
      addLog('fork', '🔀',
        `<b>fork()</b> → PID <span class="hl-pid">${ev.pid}</span> creado como hijo de PID ${ev.ppid}`)
      break
    }

    case 'exec': {
      const c = children.find(c => c.pid === ev.pid)
      if (c) { c.command = ev.command; c.state = 'execing' }
      currentStep.value = 'exec'
      doneSteps.add('fork')
      addLog('exec', '🔄',
        `<b>exec("${ev.command}")</b> → PID <span class="hl-pid">${ev.pid}</span> reemplazado por <b>${ev.command}</b>`)
      break
    }

    case 'wait': {
      shell.state = 'waiting'
      currentStep.value = 'wait'
      doneSteps.add('exec')
      addLog('wait', '⏳',
        `PID <span class="hl-pid">${ev.pid}</span> llama <b>wait()</b> — esperando a PID <span class="hl-pid">${ev.waitFor}</span>`)
      break
    }

    case 'background-start': {
      // Fase 7: padre NO llama wait(), sigue activo
      shell.state = 'running'
      const bgChild = children.find(c => c.pid === ev.pid)
      if (bgChild) bgChild.isBackground = true
      currentStep.value = 'running'
      doneSteps.add('exec')
      addLog('run', '🔀',
        `<b>Sin wait()</b> — PID <span class="hl-pid">${ev.shellPid}</span> continúa, PID <span class="hl-pid">${ev.pid}</span> corre en <b>segundo plano</b>`)
      break
    }

    case 'running': {
      const c = children.find(c => c.pid === ev.pid)
      if (c) { c.state = 'running'; animateProgress(c) }
      currentStep.value = 'running'
      doneSteps.add('wait')
      addLog('run', '🏃',
        `PID <span class="hl-pid">${ev.pid}</span> ejecutando <b>${ev.command}</b>...`)
      break
    }

    case 'pipe-running': {
      currentStep.value = 'running'
      addLog('run', '🔗',
        `PIPE activo: PID <span class="hl-pid">${ev.pid1}</span> → PID <span class="hl-pid">${ev.pid2}</span>`)
      break
    }

    case 'exit': {
      const idx = children.findIndex(c => c.pid === ev.pid)
      if (idx !== -1) {
        children[idx].state = 'exited'
        children[idx].progress = 100
        setTimeout(() => {
          const i = children.findIndex(c => c.pid === ev.pid)
          if (i !== -1) children.splice(i, 1)
        }, 1400)
      }
      currentStep.value = 'exit'
      doneSteps.add('running')
      addLog('exit', '✅',
        `PID <span class="hl-pid">${ev.pid}</span> llama <b>exit(${ev.exitCode ?? 0})</b> — proceso terminado`)
      break
    }

    case 'resume': {
      shell.state = 'idle'
      currentStep.value = ''
      doneSteps.add('exit')
      addLog('resume', '▶️',
        `PID <span class="hl-pid">${ev.pid}</span> reanuda — shell listo`)
      setTimeout(() => doneSteps.clear(), 2500)
      break
    }
  }
}
</script>

<style scoped>
.proc-anim {
  flex: 1;
  display: flex;
  flex-direction: column;
  background: #0d1b2a;
  overflow: hidden;
  font-family: 'Segoe UI', system-ui, sans-serif;
  font-size: 0.8rem;
}

/* ── Escenario ─────────────────────────────────── */
.stage {
  flex-shrink: 0;
  padding: 12px 16px 8px;
  min-height: 180px;
  display: flex;
  flex-direction: column;
  align-items: center;
  gap: 8px;
  border-bottom: 1px solid #1c2e40;
}

.stage-label {
  font-size: 0.65rem;
  color: #4a6080;
  text-transform: uppercase;
  letter-spacing: 1px;
  margin-bottom: 4px;
}

.children-area {
  display: flex;
  gap: 12px;
  flex-wrap: wrap;
  justify-content: center;
}

.child-slot {
  display: flex;
  flex-direction: column;
  align-items: center;
  gap: 4px;
}

/* ── Tarjetas de proceso ───────────────────────── */
.proc-card {
  position: relative;
  display: flex;
  align-items: center;
  gap: 10px;
  padding: 10px 14px;
  border-radius: 10px;
  border: 2px solid #1c2e40;
  background: rgba(255,255,255,0.03);
  min-width: 180px;
  transition: border-color 0.3s, box-shadow 0.3s;
  overflow: hidden;
}

.proc-card.manager { min-width: 220px; }

/* Estado → color del borde */
.proc-card.idle    { border-color: #30363d; }
.proc-card.running { border-color: #56d364; box-shadow: 0 0 12px rgba(86,211,100,.25); }
.proc-card.waiting { border-color: #e3b341; box-shadow: 0 0 12px rgba(227,179,65,.2); animation: breathe 2s ease-in-out infinite; }
.proc-card.created { border-color: #ffa657; box-shadow: 0 0 10px rgba(255,166,87,.2); }
.proc-card.execing { border-color: #58a6ff; box-shadow: 0 0 10px rgba(88,166,255,.2); }
.proc-card.exited  { border-color: #f85149; opacity: 0.7; }

@keyframes breathe {
  0%,100% { box-shadow: 0 0 8px rgba(227,179,65,.2); }
  50%      { box-shadow: 0 0 20px rgba(227,179,65,.4); }
}

.card-avatar {
  font-size: 1.6rem;
  line-height: 1;
}

.card-body {
  display: flex;
  flex-direction: column;
  gap: 2px;
  flex: 1;
}

.card-name {
  font-weight: 600;
  color: #e6edf3;
  font-size: 0.82rem;
}

.card-pid {
  font-size: 0.65rem;
  color: #4a6080;
  font-family: 'Courier New', monospace;
}

/* Badges de estado */
.state-badge {
  display: inline-block;
  padding: 1px 7px;
  border-radius: 20px;
  font-size: 0.6rem;
  font-weight: 700;
  letter-spacing: 0.5px;
}
.state-badge.idle    { background: #21262d; color: #8b949e; }
.state-badge.running { background: rgba(86,211,100,.15); color: #56d364; }
.state-badge.waiting { background: rgba(227,179,65,.15); color: #e3b341; }
.state-badge.created { background: rgba(255,166,87,.15); color: #ffa657; }
.state-badge.execing { background: rgba(88,166,255,.15); color: #58a6ff; }
.state-badge.exited  { background: rgba(248,81,73,.15);  color: #f85149; }

/* Progress bar */
.progress-wrap {
  display: flex;
  align-items: center;
  gap: 6px;
  margin-top: 6px;
}
.progress-bar {
  flex: 1;
  height: 4px;
  background: #1c2e40;
  border-radius: 2px;
  overflow: hidden;
}
.progress-fill {
  height: 100%;
  background: linear-gradient(90deg, #56d364, #7ee787);
  border-radius: 2px;
  transition: width 0.15s ease;
}
.progress-pct {
  font-size: 0.6rem;
  color: #56d364;
  font-family: 'Courier New', monospace;
  min-width: 28px;
}

/* Pulse ring (manager running) */
.pulse-ring {
  position: absolute;
  top: 50%; left: 50%;
  width: 100%; height: 100%;
  border-radius: 10px;
  border: 2px solid #56d364;
  transform: translate(-50%, -50%);
  animation: pulseRing 1.2s ease-out infinite;
  pointer-events: none;
}
@keyframes pulseRing {
  0%   { opacity: 0.7; transform: translate(-50%,-50%) scale(1); }
  100% { opacity: 0; transform: translate(-50%,-50%) scale(1.15); }
}

/* ZZZ (manager waiting) */
.zzz-wrap {
  display: flex;
  gap: 1px;
  align-items: flex-end;
  margin-left: 4px;
}
.zzz {
  color: #e3b341;
  animation: floatUp 2s ease-in-out infinite;
  font-size: 0.75rem;
  font-weight: bold;
}
.zzz.d1 { animation-delay: 0.3s; font-size: 0.9rem; }
.zzz.d2 { animation-delay: 0.6s; font-size: 1.1rem; }
@keyframes floatUp {
  0%,100% { opacity: 0; transform: translateY(0); }
  50%      { opacity: 1; transform: translateY(-6px); }
}

/* Fork arrow */
.fork-line {
  display: flex;
  flex-direction: column;
  align-items: center;
  gap: 2px;
}
.fork-line::before {
  content: '';
  display: block;
  width: 2px;
  height: 20px;
  background: linear-gradient(#ffa657, #58a6ff);
  border-radius: 1px;
}
.fork-label {
  font-size: 0.6rem;
  color: #ffa657;
  text-align: center;
  font-family: 'Courier New', monospace;
}

/* Pipe tag */
.pipe-tag {
  position: absolute;
  top: 4px;
  right: 6px;
  font-size: 0.55rem;
  color: #79c0ff;
  background: rgba(88,166,255,.1);
  padding: 1px 5px;
  border-radius: 4px;
}

/* Background tag */
.bg-tag {
  position: absolute;
  bottom: 4px;
  right: 6px;
  font-size: 0.55rem;
  color: #a5d6ff;
  background: rgba(165,214,255,.12);
  padding: 1px 5px;
  border-radius: 4px;
}

/* ── Transiciones de hijos ──────────────────────── */
.child-anim-enter-active { transition: all 0.35s ease; }
.child-anim-leave-active { transition: all 0.4s ease; }
.child-anim-enter-from  { opacity: 0; transform: translateY(-20px) scale(0.85); }
.child-anim-leave-to    { opacity: 0; transform: scale(0.8) translateY(10px); }

/* ── Ciclo de vida (steps) ─────────────────────── */
.lifecycle {
  flex-shrink: 0;
  display: flex;
  align-items: flex-start;
  justify-content: space-around;
  padding: 8px 10px;
  border-bottom: 1px solid #1c2e40;
  gap: 4px;
}

.step {
  display: flex;
  flex-direction: column;
  align-items: center;
  gap: 2px;
  padding: 5px 8px;
  border-radius: 6px;
  border: 1px solid transparent;
  transition: all 0.25s;
  flex: 1;
}

.step-icon { font-size: 1.1rem; opacity: 0.35; transition: opacity 0.25s; }
.step-key  { font-size: 0.6rem; color: #4a6080; font-family: 'Courier New', monospace; transition: color 0.25s; }
.step-desc { font-size: 0.55rem; color: #30363d; }

.step.active {
  background: rgba(88,166,255,.08);
  border-color: #58a6ff;
}
.step.active .step-icon { opacity: 1; }
.step.active .step-key  { color: #58a6ff; }
.step.active .step-desc { color: #8b949e; }

.step.done .step-icon { opacity: 0.7; }
.step.done .step-key  { color: #56d364; }

/* ── Log de syscalls ───────────────────────────── */
.syscall-log {
  flex: 1;
  display: flex;
  flex-direction: column;
  overflow: hidden;
  border-bottom: 1px solid #1c2e40;
  min-height: 0;
}

.log-head {
  padding: 6px 12px;
  font-size: 0.65rem;
  color: #4a6080;
  text-transform: uppercase;
  letter-spacing: 1px;
  background: #091624;
  flex-shrink: 0;
}

.log-body {
  flex: 1;
  overflow-y: auto;
  padding: 6px 12px;
  font-family: 'Courier New', monospace;
}

.log-empty {
  color: #30363d;
  font-style: italic;
  padding: 12px 0;
  font-size: 0.75rem;
}

.log-line {
  display: flex;
  align-items: baseline;
  gap: 6px;
  padding: 2px 0;
  border-bottom: 1px solid rgba(255,255,255,0.02);
  line-height: 1.4;
}

.log-time {
  font-size: 0.6rem;
  color: #30363d;
  flex-shrink: 0;
}
.log-icon { flex-shrink: 0; }
.log-text { color: #8b949e; font-size: 0.72rem; }

/* Colores por tipo de entrada */
.log-line.fork   .log-text { color: #ffa657; }
.log-line.exec   .log-text { color: #58a6ff; }
.log-line.wait   .log-text { color: #e3b341; }
.log-line.run    .log-text { color: #56d364; }
.log-line.exit   .log-text { color: #f85149; }
.log-line.resume .log-text { color: #79c0ff; }

/* Transición de entradas */
.log-entry-enter-active { transition: all 0.2s ease; }
.log-entry-enter-from   { opacity: 0; transform: translateX(-10px); }

/* ── Leyenda ────────────────────────────────────── */
.legend {
  flex-shrink: 0;
  display: flex;
  flex-wrap: wrap;
  gap: 8px;
  padding: 6px 12px;
  background: #091624;
}

.leg-item {
  display: flex;
  align-items: center;
  gap: 4px;
}
.leg-dot {
  width: 8px;
  height: 8px;
  border-radius: 50%;
}
.leg-label {
  font-size: 0.6rem;
  color: #4a6080;
}

/* Highlight de PIDs en el log */
:deep(.hl-pid) {
  color: #79c0ff;
  font-weight: bold;
}
</style>
