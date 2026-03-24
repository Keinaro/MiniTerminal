<template>
  <div
    class="widget"
    :class="{
      'widget--minimized': isMinimized,
      'widget--maximized': isMaximized,
    }"
    :style="widgetStyle"
    @mousedown="emit('focus')"
  >
    <!-- ── Barra de título ──────────────────────────── -->
    <div
      class="titlebar"
      :style="{ borderTopColor: accentColor }"
      @mousedown="onTitlebarMousedown"
      @dblclick="toggleMaximize"
    >
      <!-- Controles estilo macOS -->
      <div class="tb-controls">
        <button class="wctrl wctrl--close"    title="Cerrar"    @mousedown.stop @click="emit('close')" />
        <button class="wctrl wctrl--minimize" title="Minimizar" @mousedown.stop @click="toggleMinimize" />
        <button class="wctrl wctrl--maximize" title="Maximizar" @mousedown.stop @click="toggleMaximize" />
      </div>

      <!-- Título centrado -->
      <div class="tb-title">
        <span v-if="icon" class="tb-icon">{{ icon }}</span>
        <span class="tb-text">{{ title }}</span>
        <span v-if="isMinimized" class="tb-tag">minimizado</span>
      </div>

      <!-- Espejo invisible para centrar el título -->
      <div class="tb-controls tb-mirror" aria-hidden="true">
        <span class="wctrl" /><span class="wctrl" /><span class="wctrl" />
      </div>
    </div>

    <!-- ── Cuerpo ────────────────────────────────────── -->
    <div v-show="!isMinimized" class="widget-body">
      <slot />
    </div>

    <!-- ── Resize handles (8 dir) ───────────────────── -->
    <template v-if="!isMinimized && !isMaximized">
      <div v-for="d in DIRS" :key="d"
        :class="`rh rh-${d}`"
        @mousedown.stop.prevent="onResizeStart($event, d)"
      />
    </template>
  </div>
</template>

<script setup>
import { ref, computed, onUnmounted } from 'vue'

const props = defineProps({
  title:       { type: String, default: 'Widget'   },
  icon:        { type: String, default: ''         },
  accentColor: { type: String, default: '#58a6ff'  },
  initialX:    { type: Number, default: 40         },
  initialY:    { type: Number, default: 40         },
  initialW:    { type: Number, default: 600        },
  initialH:    { type: Number, default: 480        },
  minW:        { type: Number, default: 280        },
  minH:        { type: Number, default: 180        },
  zIndex:      { type: Number, default: 1          },
})

const emit = defineEmits(['focus', 'close'])

/* ── Estado ─────────────────────────────────────────── */
const x = ref(props.initialX)
const y = ref(props.initialY)
const w = ref(props.initialW)
const h = ref(props.initialH)
const isMinimized = ref(false)
const isMaximized = ref(false)

// Estado guardado antes de maximizar
const saved = ref({ x: props.initialX, y: props.initialY, w: props.initialW, h: props.initialH })

const DIRS = ['n','ne','e','se','s','sw','w','nw']

/* ── Style computado ────────────────────────────────── */
const widgetStyle = computed(() => {
  if (isMaximized.value) {
    return { left: '0', top: '0', width: '100%', height: '100%', zIndex: props.zIndex }
  }
  return {
    left:   `${x.value}px`,
    top:    `${y.value}px`,
    width:  `${w.value}px`,
    height: isMinimized.value ? 'auto' : `${h.value}px`,
    zIndex: props.zIndex,
  }
})

/* ── Minimize / Maximize ────────────────────────────── */
function toggleMinimize() {
  if (isMaximized.value) isMaximized.value = false
  isMinimized.value = !isMinimized.value
  emit('focus')
}

function toggleMaximize() {
  if (isMinimized.value) isMinimized.value = false
  if (!isMaximized.value) {
    saved.value = { x: x.value, y: y.value, w: w.value, h: h.value }
    isMaximized.value = true
  } else {
    Object.assign({ x, y, w, h }, {})   // dummy — usar línea de abajo
    x.value = saved.value.x
    y.value = saved.value.y
    w.value = saved.value.w
    h.value = saved.value.h
    isMaximized.value = false
  }
  emit('focus')
}

/* ── Drag ───────────────────────────────────────────── */
let dStartMX = 0, dStartMY = 0, dStartX = 0, dStartY = 0

function onTitlebarMousedown(e) {
  if (e.button !== 0 || isMaximized.value) return
  emit('focus')
  dStartMX = e.clientX;  dStartMY = e.clientY
  dStartX  = x.value;    dStartY  = y.value
  document.addEventListener('mousemove', onDragMove)
  document.addEventListener('mouseup',   onDragUp)
  // Evitar selección de texto del canvas durante drag
  document.body.style.userSelect = 'none'
}

function onDragMove(e) {
  x.value = dStartX + (e.clientX - dStartMX)
  y.value = Math.max(0, dStartY + (e.clientY - dStartMY))
}

function onDragUp() {
  document.removeEventListener('mousemove', onDragMove)
  document.removeEventListener('mouseup',   onDragUp)
  document.body.style.userSelect = ''
}

/* ── Resize ─────────────────────────────────────────── */
let rDir = '', rMX = 0, rMY = 0, rX = 0, rY = 0, rW = 0, rH = 0

function onResizeStart(e, dir) {
  if (e.button !== 0) return
  emit('focus')
  rDir = dir;  rMX = e.clientX;  rMY = e.clientY
  rX = x.value;  rY = y.value;  rW = w.value;  rH = h.value
  document.addEventListener('mousemove', onResizeMove)
  document.addEventListener('mouseup',   onResizeUp)
  document.body.style.userSelect = 'none'
}

function onResizeMove(e) {
  const dx = e.clientX - rMX
  const dy = e.clientY - rMY

  if (rDir.includes('e')) w.value = Math.max(props.minW, rW + dx)

  if (rDir.includes('w')) {
    const nw = Math.max(props.minW, rW - dx)
    x.value  = rX + (rW - nw)
    w.value  = nw
  }

  if (rDir.includes('s')) h.value = Math.max(props.minH, rH + dy)

  if (rDir.includes('n')) {
    const nh = Math.max(props.minH, rH - dy)
    y.value  = Math.max(0, rY + (rH - nh))
    h.value  = nh
  }
}

function onResizeUp() {
  document.removeEventListener('mousemove', onResizeMove)
  document.removeEventListener('mouseup',   onResizeUp)
  document.body.style.userSelect = ''
}

onUnmounted(() => {
  document.removeEventListener('mousemove', onDragMove)
  document.removeEventListener('mouseup',   onDragUp)
  document.removeEventListener('mousemove', onResizeMove)
  document.removeEventListener('mouseup',   onResizeUp)
  document.body.style.userSelect = ''
})
</script>

<style scoped>
/* ── Widget raíz ─────────────────────────────────────
   overflow: visible para que los handles de resize
   puedan sobresalir del borde. El ::before recrea
   el fondo/borde visualmente. ─────────────────────── */
.widget {
  position: absolute;
  display: flex;
  flex-direction: column;
  overflow: visible;   /* permite que los handles sobresalgan */
  border-radius: 10px;
}

/* Fondo/borde visual del widget (no clip) */
.widget::before {
  content: '';
  position: absolute;
  inset: 0;
  border-radius: 10px;
  background: rgba(22, 27, 34, 0.96);
  border: 1px solid #30363d;
  box-shadow:
    0 20px 60px rgba(0, 0, 0, 0.55),
    0 0 0 1px rgba(255,255,255,0.04);
  z-index: -1;
  pointer-events: none;
  transition: box-shadow 0.2s;
}

/* Cuando es el widget con mayor z-index (foco visual) */
.widget:focus-within::before,
.widget:hover::before {
  box-shadow:
    0 24px 80px rgba(0, 0, 0, 0.65),
    0 0 0 1px rgba(255,255,255,0.08);
}

.widget--maximized {
  border-radius: 0;
}
.widget--maximized::before {
  border-radius: 0;
}

/* ── Barra de título ─────────────────────────────────── */
.titlebar {
  display: flex;
  align-items: center;
  justify-content: space-between;
  padding: 0 10px;
  height: 38px;
  background: rgba(22, 27, 34, 0.98);
  border-bottom: 1px solid #30363d;
  border-top: 3px solid transparent; /* sobreescrito por :style */
  border-radius: 9px 9px 0 0;
  flex-shrink: 0;
  cursor: grab;
  position: relative;
  z-index: 1;
}

.titlebar:active { cursor: grabbing; }

.widget--maximized .titlebar {
  border-radius: 0;
}

/* ── Controles macOS ─────────────────────────────────── */
.tb-controls {
  display: flex;
  gap: 7px;
  align-items: center;
  width: 52px;
  flex-shrink: 0;
}

.tb-mirror { visibility: hidden; }

.wctrl {
  width: 13px;
  height: 13px;
  border-radius: 50%;
  border: none;
  cursor: pointer;
  padding: 0;
  background: #3d4450;
  transition: filter 0.12s, transform 0.1s;
  flex-shrink: 0;
}

.wctrl:hover { filter: brightness(1.25); transform: scale(1.08); }
.wctrl--close    { background: #ff5f57; }
.wctrl--minimize { background: #ffbd2e; }
.wctrl--maximize { background: #28c840; }

/* ── Título ──────────────────────────────────────────── */
.tb-title {
  flex: 1;
  display: flex;
  align-items: center;
  justify-content: center;
  gap: 6px;
  min-width: 0;
}

.tb-icon { font-size: 0.85rem; flex-shrink: 0; }

.tb-text {
  font-size: 0.72rem;
  color: #8b949e;
  font-family: 'Segoe UI', system-ui, sans-serif;
  white-space: nowrap;
  overflow: hidden;
  text-overflow: ellipsis;
}

.tb-tag {
  font-size: 0.6rem;
  padding: 1px 6px;
  background: rgba(255,189,46,0.15);
  color: #ffbd2e;
  border-radius: 10px;
  font-family: sans-serif;
}

/* ── Cuerpo del widget ───────────────────────────────── */
.widget-body {
  flex: 1;
  display: flex;
  flex-direction: column;
  min-height: 0;
  overflow: hidden;
  border-radius: 0 0 9px 9px;
  position: relative;
  z-index: 1;
  /* Permitir selección de texto en los hijos (la terminal lo necesita) */
  user-select: text;
}

/* ── Resize handles ──────────────────────────────────── */
.rh {
  position: absolute;
  z-index: 20;
  /* Descomenta para debug: background: rgba(255,0,0,0.2); */
}

/* Cardinal */
.rh-n  { top: -4px;    left: 14px;  right: 14px; height: 8px;  cursor: n-resize;    }
.rh-s  { bottom: -4px; left: 14px;  right: 14px; height: 8px;  cursor: s-resize;    }
.rh-e  { right: -4px;  top: 14px; bottom: 14px;  width: 8px;   cursor: e-resize;    }
.rh-w  { left: -4px;   top: 14px; bottom: 14px;  width: 8px;   cursor: w-resize;    }

/* Esquinas */
.rh-ne { top: -5px;    right: -5px;  width: 18px; height: 18px; cursor: ne-resize;  }
.rh-nw { top: -5px;    left: -5px;   width: 18px; height: 18px; cursor: nw-resize;  }
.rh-se { bottom: -5px; right: -5px;  width: 18px; height: 18px; cursor: se-resize;  }
.rh-sw { bottom: -5px; left: -5px;   width: 18px; height: 18px; cursor: sw-resize;  }
</style>
