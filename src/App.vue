<template>
  <div class="app">
    <header class="header">
      <div class="header-left">
        <span class="header-icon">⬛</span>
        <h1>MiniTerminal<span class="accent">_SyJ</span></h1>
      </div>
      <div class="header-right">
        <span class="badge">Sistemas Operativos</span>
        <span class="badge">Universidad Rafael Landívar</span>
      </div>
    </header>

    <main class="main">
      <!-- Panel izquierdo: Terminal -->
      <div class="panel">
        <div class="panel-header">
          <div class="win-controls">
            <span class="dot red"></span>
            <span class="dot yellow"></span>
            <span class="dot green"></span>
          </div>
          <span class="panel-title">bash — MiniTerminal_SyJ</span>
        </div>
        <TerminalWidget @process-event="onProcessEvent" />
      </div>

      <div class="divider"></div>

      <!-- Panel derecho: Animación de procesos -->
      <div class="panel">
        <div class="panel-header">
          <span class="panel-icon">🔬</span>
          <span class="panel-title">Visualizador de Procesos del SO</span>
        </div>
        <ProcessAnimation :events="processEvents" />
      </div>
    </main>
  </div>
</template>

<script setup>
import { ref } from 'vue'
import TerminalWidget from './components/TerminalWidget.vue'
import ProcessAnimation from './components/ProcessAnimation.vue'

const processEvents = ref([])

function onProcessEvent(event) {
  processEvents.value = [...processEvents.value, { ...event, id: Date.now() + Math.random() }]
}
</script>

<style scoped>
.app {
  height: 100vh;
  display: flex;
  flex-direction: column;
  background: #0d1117;
}

.header {
  display: flex;
  justify-content: space-between;
  align-items: center;
  padding: 8px 16px;
  background: #161b22;
  border-bottom: 1px solid #30363d;
  flex-shrink: 0;
  height: 44px;
}

.header-left {
  display: flex;
  align-items: center;
  gap: 8px;
}

h1 {
  font-size: 0.95rem;
  color: #e6edf3;
  font-weight: 600;
  letter-spacing: 0.5px;
}

.accent { color: #58a6ff; }

.header-right {
  display: flex;
  gap: 8px;
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

.main {
  flex: 1;
  display: flex;
  overflow: hidden;
  min-height: 0;
}

.panel {
  flex: 1;
  display: flex;
  flex-direction: column;
  overflow: hidden;
  min-width: 0;
}

.divider {
  width: 3px;
  background: #30363d;
  flex-shrink: 0;
}

.panel-header {
  display: flex;
  align-items: center;
  gap: 8px;
  padding: 8px 12px;
  background: #161b22;
  border-bottom: 1px solid #30363d;
  flex-shrink: 0;
  height: 38px;
}

.win-controls {
  display: flex;
  gap: 6px;
  align-items: center;
}

.dot {
  width: 12px;
  height: 12px;
  border-radius: 50%;
}
.dot.red    { background: #ff5f57; }
.dot.yellow { background: #ffbd2e; }
.dot.green  { background: #28c840; }

.panel-title {
  font-size: 0.75rem;
  color: #8b949e;
  font-family: sans-serif;
}

.panel-icon { font-size: 0.9rem; }
</style>
