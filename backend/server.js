const express = require('express');
const { spawn } = require('child_process');
const { randomUUID } = require('crypto');
const fs = require('fs');
const os = require('os');
const path = require('path');

const PORT = Number(process.env.PORT || 3001);
const SHELL_BINARY = process.env.SHELL_BINARY || '/srv/mi_terminal';
const SHELL_CWD = process.env.SHELL_CWD || '/srv';
const PROMPT = 'MiniTerminal_SyJ> ';

const app = express();
app.use(express.json());

class ShellSession {
  constructor(id) {
    this.id = id;
    this.buffer = '';
    this.closed = false;
    this.waiters = [];
    this.execChain = Promise.resolve();
    this.workDir = fs.mkdtempSync(path.join(os.tmpdir(), 'miniterminal-'));

    this.proc = spawn(SHELL_BINARY, [], {
      cwd: this.workDir,
      stdio: ['pipe', 'pipe', 'pipe'],
    });

    this.proc.stdout.on('data', (chunk) => {
      this.buffer += chunk.toString();
      this.flushWaiters();
    });

    this.proc.stderr.on('data', (chunk) => {
      this.buffer += chunk.toString();
      this.flushWaiters();
    });

    this.proc.on('close', () => {
      this.closed = true;
      this.flushClosed();
    });

    this.ready = this.readUntilPrompt(10000);
  }

  async syncVfs(entries = []) {
    if (this.closed) {
      throw new Error('Sesion cerrada');
    }

    if (!Array.isArray(entries)) {
      throw new Error('entries debe ser arreglo');
    }

    this.execChain = this.execChain.then(async () => {
      fs.rmSync(this.workDir, { recursive: true, force: true });
      fs.mkdirSync(this.workDir, { recursive: true });

      for (const entry of entries) {
        if (!entry || typeof entry.path !== 'string' || typeof entry.type !== 'string') {
          continue;
        }

        const rel = entry.path.replace(/\\/g, '/').replace(/^\/+/, '');
        if (!rel || rel.includes('..')) {
          continue;
        }

        const abs = path.resolve(this.workDir, rel);
        if (!abs.startsWith(path.resolve(this.workDir))) {
          continue;
        }

        if (entry.type === 'dir') {
          fs.mkdirSync(abs, { recursive: true });
          continue;
        }

        if (entry.type === 'file') {
          fs.mkdirSync(path.dirname(abs), { recursive: true });
          const content = typeof entry.content === 'string' ? entry.content : '';
          fs.writeFileSync(abs, content, 'utf8');
        }
      }

      this.proc.stdin.write(`cd ${this.workDir}\n`);
      await this.readUntilPrompt(10000);
    });

    return this.execChain;
  }

  flushWaiters() {
    while (this.waiters.length > 0) {
      const idx = this.buffer.indexOf(PROMPT);
      if (idx === -1) break;

      const waiter = this.waiters.shift();
      clearTimeout(waiter.timer);
      const out = this.buffer.slice(0, idx);
      this.buffer = this.buffer.slice(idx + PROMPT.length);
      waiter.resolve(out.replace(/\r/g, ''));
    }
  }

  flushClosed() {
    while (this.waiters.length > 0) {
      const waiter = this.waiters.shift();
      clearTimeout(waiter.timer);
      waiter.resolve(this.buffer.replace(/\r/g, ''));
    }
    this.buffer = '';
  }

  readUntilPrompt(timeoutMs = 8000) {
    return new Promise((resolve, reject) => {
      const waiter = { resolve, reject, timer: null };
      const timer = setTimeout(() => {
        const idx = this.waiters.indexOf(waiter);
        if (idx !== -1) this.waiters.splice(idx, 1);
        reject(new Error('Timeout esperando prompt del shell C++'));
      }, timeoutMs);

      waiter.timer = timer;
      this.waiters.push(waiter);
      this.flushWaiters();
    });
  }

  enqueue(command) {
    this.execChain = this.execChain.then(() => this.execute(command));
    return this.execChain;
  }

  async execute(command) {
    if (this.closed) {
      throw new Error('Sesion cerrada');
    }

    const trimmed = command.trim();
    this.proc.stdin.write(command + '\n');

    if (trimmed === 'exit') {
      const out = await new Promise((resolve) => {
        const timer = setTimeout(() => resolve(this.buffer.replace(/\r/g, '')), 3000);
        this.proc.once('close', () => {
          clearTimeout(timer);
          resolve(this.buffer.replace(/\r/g, ''));
        });
      });
      this.closed = true;
      this.buffer = '';
      return { output: out.trimEnd(), ended: true };
    }

    try {
      const output = await this.readUntilPrompt(10000);
      return { output: output.trimEnd(), ended: false };
    } catch (err) {
      if (String(err.message || err).includes('Timeout esperando prompt')) {
        // Comandos interactivos (ej. cat > archivo) bloquean sin TTY.
        // Reiniciamos la sesión para evitar que quede colgada.
        this.close();
        return {
          output: 'Comando interactivo no soportado en modo web (requiere entrada continua). Usa echo "texto" > archivo.txt o cat < archivo.txt.',
          ended: true,
        };
      }
      throw err;
    }
  }

  close() {
    if (!this.closed) {
      this.proc.kill();
      this.closed = true;
    }
    try {
      fs.rmSync(this.workDir, { recursive: true, force: true });
    } catch {
      // noop
    }
  }
}

const sessions = new Map();

function buildSnapshotEntries(baseDir) {
  const entries = [];

  function walk(currentDir, relPrefix) {
    const items = fs.readdirSync(currentDir, { withFileTypes: true });
    for (const item of items) {
      const relPath = relPrefix ? `${relPrefix}/${item.name}` : item.name;
      const absPath = path.join(currentDir, item.name);

      if (item.isDirectory()) {
        entries.push({ path: relPath, type: 'dir' });
        walk(absPath, relPath);
        continue;
      }

      if (item.isFile()) {
        let content = '';
        try {
          const stat = fs.statSync(absPath);
          if (stat.size <= 512 * 1024) {
            content = fs.readFileSync(absPath, 'utf8');
          }
        } catch {
          content = '';
        }

        entries.push({ path: relPath, type: 'file', content });
      }
    }
  }

  walk(baseDir, '');
  return entries;
}

app.get('/health', (_req, res) => {
  res.json({ ok: true, sessions: sessions.size });
});

app.post('/session', async (_req, res) => {
  try {
    const id = randomUUID();
    const session = new ShellSession(id);
    await session.ready;
    sessions.set(id, session);
    res.json({ sessionId: id });
  } catch (err) {
    res.status(500).json({ error: String(err.message || err) });
  }
});

app.post('/execute', async (req, res) => {
  const { sessionId, command } = req.body || {};

  if (!sessionId || typeof sessionId !== 'string') {
    return res.status(400).json({ error: 'sessionId requerido' });
  }
  if (typeof command !== 'string') {
    return res.status(400).json({ error: 'command debe ser string' });
  }

  const session = sessions.get(sessionId);
  if (!session) {
    return res.status(404).json({ error: 'Sesion no encontrada' });
  }

  try {
    const result = await session.enqueue(command);
    if (result.ended) {
      sessions.delete(sessionId);
    }
    return res.json(result);
  } catch (err) {
    if (session.closed) {
      sessions.delete(sessionId);
    }
    return res.status(500).json({ error: String(err.message || err) });
  }
});

app.post('/sync-vfs', async (req, res) => {
  const { sessionId, entries } = req.body || {};

  if (!sessionId || typeof sessionId !== 'string') {
    return res.status(400).json({ error: 'sessionId requerido' });
  }

  const session = sessions.get(sessionId);
  if (!session) {
    return res.status(404).json({ error: 'Sesion no encontrada' });
  }

  try {
    await session.syncVfs(entries);
    return res.json({ ok: true });
  } catch (err) {
    return res.status(500).json({ error: String(err.message || err) });
  }
});

app.post('/snapshot', (req, res) => {
  const { sessionId } = req.body || {};

  if (!sessionId || typeof sessionId !== 'string') {
    return res.status(400).json({ error: 'sessionId requerido' });
  }

  const session = sessions.get(sessionId);
  if (!session) {
    return res.status(404).json({ error: 'Sesion no encontrada' });
  }

  try {
    const entries = buildSnapshotEntries(session.workDir);
    return res.json({ entries });
  } catch (err) {
    return res.status(500).json({ error: String(err.message || err) });
  }
});

app.post('/session/close', (req, res) => {
  const { sessionId } = req.body || {};
  if (!sessionId || typeof sessionId !== 'string') {
    return res.status(400).json({ error: 'sessionId requerido' });
  }

  const session = sessions.get(sessionId);
  if (session) {
    session.close();
    sessions.delete(sessionId);
  }

  return res.json({ ok: true });
});

process.on('SIGTERM', () => {
  for (const s of sessions.values()) s.close();
  process.exit(0);
});

app.listen(PORT, () => {
  console.log(`Backend C++ escuchando en puerto ${PORT}`);
});
