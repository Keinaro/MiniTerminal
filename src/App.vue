<template>
  <div class="app">
    <!-- ── Header ─────────────────────────────────── -->
    <header class="header">
      <div class="header-left">
        <span>⬛</span>
        <h1>MiniTerminal<span class="accent">_SyJ</span></h1>
        <div class="username-wrap" title="Cambiar nombre de usuario">
          <span class="username-at">@</span>
          <input
            v-model="usernameModel"
            class="username-input"
            maxlength="20"
            spellcheck="false"
            autocomplete="off"
          />
        </div>
      </div>

      <div class="header-center">
        <!-- Botones para reabrir widgets cerrados -->
        <button
          v-if="!widgets.terminal.visible"
          class="reopen-btn"
          @click="widgets.terminal.visible = true; bringToFront('terminal')"
        >⬛ Terminal</button>
        <button
          v-if="!widgets.procesos.visible"
          class="reopen-btn"
          @click="widgets.procesos.visible = true; bringToFront('procesos')"
        >🔬 Procesos</button>
        <button
          v-if="!widgets.explorador.visible"
          class="reopen-btn"
          @click="widgets.explorador.visible = true; bringToFront('explorador')"
        >📁 Explorador</button>
      </div>

      <div class="header-right">
        <span class="badge">Sistemas Operativos</span>
        <span class="badge">Univ. Rafael Landívar</span>

        <!-- Selector de tema del lienzo -->
        <div class="theme-picker">
          <button
            v-for="t in themes"
            :key="t.id"
            class="theme-dot"
            :class="{ 'theme-dot--active': canvasTheme === t.id }"
            :title="t.label"
            :style="{ background: t.preview }"
            @click="canvasTheme = t.id"
          />
        </div>
      </div>
    </header>

    <!-- ── Canvas: lienzo ──────────────────────────── -->
    <main class="canvas" :class="`canvas--${canvasTheme}`">

      <!-- Widget: Terminal -->
      <Widget
        v-if="widgets.terminal.visible"
        title="bash — MiniTerminal_SyJ"
        accentColor="#56d364"
        :initialX="40"
        :initialY="30"
        :initialW="680"
        :initialH="530"
        :minW="320"
        :minH="220"
        :zIndex="widgets.terminal.z"
        @focus="bringToFront('terminal')"
        @close="widgets.terminal.visible = false"
      >
        <TerminalWidget @process-event="onProcessEvent" />
      </Widget>

      <!-- Widget: Visualizador de Procesos -->
      <Widget
        v-if="widgets.procesos.visible"
        title="Visualizador de Procesos del SO"
        icon="🔬"
        accentColor="#58a6ff"
        :initialX="760"
        :initialY="30"
        :initialW="560"
        :initialH="530"
        :minW="320"
        :minH="300"
        :zIndex="widgets.procesos.z"
        @focus="bringToFront('procesos')"
        @close="widgets.procesos.visible = false"
      >
        <ProcessAnimation :events="processEvents" />
      </Widget>

      <!-- Widget: Explorador de Archivos -->
      <Widget
        v-if="widgets.explorador.visible"
        title="Explorador de Archivos"
        icon="📁"
        accentColor="#f0883e"
        :initialX="40"
        :initialY="590"
        :initialW="760"
        :initialH="380"
        :minW="420"
        :minH="240"
        :zIndex="widgets.explorador.z"
        @focus="bringToFront('explorador')"
        @close="widgets.explorador.visible = false"
      >
        <FileExplorer />
      </Widget>

    </main>
  </div>
</template>

<script setup>
import { ref, reactive, computed } from 'vue'
import TerminalWidget from './components/TerminalWidget.vue'
import ProcessAnimation from './components/ProcessAnimation.vue'
import FileExplorer from './components/FileExplorer.vue'
import Widget from './components/Widget.vue'
import { username } from './store/vfs.js'

// v-model sobre el ref del store
const usernameModel = computed({
  get: () => username.value,
  set: v => { username.value = v.trim() || 'usuario' },
})

/* ── Tema del lienzo ───────────────────────────────── */
const canvasTheme = ref('dark')

const themes = [
  { id: 'dark',  label: 'Negro',   preview: '#0d1117' },
  { id: 'gray',  label: 'Gris',    preview: '#3a3f4b' },
  { id: 'light', label: 'Blanco',  preview: '#f0f2f5' },
]

/* ── Eventos de proceso ────────────────────────────── */
const processEvents = ref([])

function onProcessEvent(event) {
  processEvents.value = [...processEvents.value, { ...event, id: Date.now() + Math.random() }]
}

/* ── Z-index management ────────────────────────────── */
let zTick = 10

const widgets = reactive({
  terminal:   { visible: true, z: 11 },
  procesos:   { visible: true, z: 10 },
  explorador: { visible: true, z: 9  },
})

function bringToFront(name) {
  zTick++
  widgets[name].z = zTick
}
</script>

<style scoped>
.app {
  height: 100vh;
  display: flex;
  flex-direction: column;
  overflow: hidden;
}

/* ── Header ──────────────────────────────────────── */
.header {
  display: flex;
  align-items: center;
  justify-content: space-between;
  padding: 0 16px;
  height: 44px;
  background: #161b22;
  border-bottom: 1px solid #30363d;
  flex-shrink: 0;
  position: relative;
  z-index: 9999;
  gap: 12px;
}

.header-left {
  display: flex;
  align-items: center;
  gap: 8px;
  flex-shrink: 0;
}

/* ── Username editable ───────────────────────────── */
.username-wrap {
  display: flex;
  align-items: center;
  gap: 2px;
  background: #0d1117;
  border: 1px solid #30363d;
  border-radius: 6px;
  padding: 2px 8px 2px 6px;
  transition: border-color 0.15s;
}
.username-wrap:focus-within { border-color: #56d364; }

.username-at {
  color: #56d364;
  font-family: 'Courier New', monospace;
  font-size: 0.82rem;
  font-weight: bold;
  line-height: 1;
  flex-shrink: 0;
}

.username-input {
  background: transparent;
  border: none;
  outline: none;
  color: #e6edf3;
  font-family: 'Courier New', monospace;
  font-size: 0.78rem;
  width: 90px;
  caret-color: #56d364;
}

h1 {
  font-size: 0.95rem;
  color: #e6edf3;
  font-weight: 600;
  font-family: 'Courier New', monospace;
}

.accent { color: #58a6ff; }

.header-center {
  display: flex;
  gap: 6px;
  flex: 1;
}

.reopen-btn {
  padding: 3px 10px;
  background: #21262d;
  border: 1px solid #30363d;
  border-radius: 6px;
  color: #8b949e;
  font-size: 0.7rem;
  cursor: pointer;
  font-family: inherit;
  transition: background 0.15s, color 0.15s;
}
.reopen-btn:hover { background: #30363d; color: #e6edf3; }

.header-right {
  display: flex;
  align-items: center;
  gap: 8px;
  flex-shrink: 0;
}

.badge {
  font-size: 0.65rem;
  padding: 2px 8px;
  background: #21262d;
  border: 1px solid #30363d;
  border-radius: 20px;
  color: #8b949e;
  font-family: sans-serif;
}

/* ── Theme picker ────────────────────────────────── */
.theme-picker {
  display: flex;
  align-items: center;
  gap: 5px;
  padding: 3px 8px;
  background: #21262d;
  border: 1px solid #30363d;
  border-radius: 20px;
}

.theme-dot {
  width: 14px;
  height: 14px;
  border-radius: 50%;
  border: 2px solid transparent;
  cursor: pointer;
  padding: 0;
  transition: border-color 0.15s, transform 0.1s;
  flex-shrink: 0;
}
.theme-dot:hover       { transform: scale(1.15); }
.theme-dot--active     { border-color: #58a6ff; transform: scale(1.2); }

/* ── Canvas ──────────────────────────────────────── */
.canvas {
  flex: 1;
  position: relative;
  overflow: hidden;
  min-height: 0;
  transition: background-color 0.3s;
}

/* ─── Tema: Negro (default) ─── */
.canvas--dark {
  background-color: #0d1117;
  background-image: radial-gradient(circle, rgba(48, 54, 61, 0.9) 1.5px, transparent 1.5px);
  background-size: 28px 28px;
}

/* ─── Tema: Gris ─── */
.canvas--gray {
  background-color: #2d333b;
  background-image: radial-gradient(circle, rgba(90, 100, 115, 0.55) 1.5px, transparent 1.5px);
  background-size: 28px 28px;
}

/* ─── Tema: Blanco ─── */
.canvas--light {
  background-color: #f0f2f5;
  background-image: radial-gradient(circle, rgba(180, 185, 195, 0.8) 1.5px, transparent 1.5px);
  background-size: 28px 28px;
}
</style>
