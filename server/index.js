// Mashup Share server: stores ONE mashup per (level, account) and serves them to everyone.
const express = require('express');
const rateLimit = require('express-rate-limit');
const fs = require('fs');
const path = require('path');
const crypto = require('crypto');

const PORT = process.env.PORT || 3000;
const DATA_DIR = process.env.DATA_DIR || path.join(__dirname, 'data');
const ADMIN_KEY = process.env.ADMIN_KEY || '';          // optional, for moderation
const MAX_BYTES = 20 * 1024 * 1024;                     // keep in sync with the mod
const FILES_DIR = path.join(DATA_DIR, 'files');
const DB_FILE = path.join(DATA_DIR, 'db.json');

fs.mkdirSync(FILES_DIR, { recursive: true });

// ---- tiny JSON "database" ----
// { tokens: { accountID: token }, mashups: { levelID: { accountID: {...} } } }
let db = { tokens: {}, mashups: {} };
if (fs.existsSync(DB_FILE)) {
  try { db = JSON.parse(fs.readFileSync(DB_FILE, 'utf8')); } catch (e) { console.error('db read failed', e); }
}
function saveDb() {
  const tmp = DB_FILE + '.tmp';
  fs.writeFileSync(tmp, JSON.stringify(db));
  fs.renameSync(tmp, DB_FILE);
}

// ---- helpers ----
const MIME = { mp3: 'audio/mpeg', ogg: 'audio/ogg', wav: 'audio/wav' };

function detectExt(buf) {
  if (buf.length < 12) return null;
  if (buf.slice(0, 3).toString('latin1') === 'ID3') return 'mp3';
  if (buf[0] === 0xff && (buf[1] & 0xe0) === 0xe0) return 'mp3';
  if (buf.slice(0, 4).toString('latin1') === 'OggS') return 'ogg';
  if (buf.slice(0, 4).toString('latin1') === 'RIFF' && buf.slice(8, 12).toString('latin1') === 'WAVE') return 'wav';
  return null;
}

const isId = (s) => /^\d{1,9}$/.test(String(s)) && Number(s) > 0;

function safeDecode(s, max) {
  try { return decodeURIComponent(String(s || '')).replace(/[\u0000-\u001f<>]/g, '').trim().slice(0, max); }
  catch { return ''; }
}

function auth(req, res, next) {
  const acc = req.get('X-Account-ID');
  const token = req.get('X-Token');
  if (!isId(acc) || !/^[0-9a-f]{32}$/.test(token || '')) return res.status(401).send('Missing credentials');

  const known = db.tokens[acc];
  if (!known) {                       // first use: bind this token to the account ID
    db.tokens[acc] = token;
    saveDb();
  } else if (!crypto.timingSafeEqual(Buffer.from(known), Buffer.from(token))) {
    return res.status(403).send('Token does not match this account');
  }
  req.accountID = String(acc);
  next();
}

const app = express();
app.set('trust proxy', 1);

const readLimiter = rateLimit({ windowMs: 60_000, limit: 120 });
const writeLimiter = rateLimit({ windowMs: 60 * 60_000, limit: 30 });

app.get('/health', (_req, res) => res.json({ ok: true }));

// List all mashups for a level (most played first)
app.get('/api/mashups/:levelID', readLimiter, (req, res) => {
  if (!isId(req.params.levelID)) return res.status(400).send('Bad level ID');
  const level = db.mashups[req.params.levelID] || {};
  const list = Object.entries(level).map(([accountID, m]) => ({
    accountID: Number(accountID),
    uploader: m.uploader,
    title: m.title,
    ext: m.ext,
    size: m.size,
    downloads: m.downloads,
    createdAt: m.createdAt,
  })).sort((a, b) => b.downloads - a.downloads || b.createdAt - a.createdAt);
  res.json({ mashups: list });
});

// Download one mashup
app.get('/api/mashups/:levelID/:accountID/file', readLimiter, (req, res) => {
  const { levelID, accountID } = req.params;
  if (!isId(levelID) || !isId(accountID)) return res.status(400).send('Bad ID');
  const m = (db.mashups[levelID] || {})[accountID];
  if (!m) return res.status(404).send('Not found');
  const file = path.join(FILES_DIR, levelID, `${accountID}.${m.ext}`);
  if (!fs.existsSync(file)) return res.status(404).send('File missing');
  m.downloads++;
  saveDb();
  res.type(MIME[m.ext]).sendFile(file);
});

// Upload (or replace) YOUR mashup for a level
app.post('/api/mashups/:levelID',
  writeLimiter,
  express.raw({ type: '*/*', limit: MAX_BYTES }),
  auth,
  (req, res) => {
    const { levelID } = req.params;
    if (!isId(levelID)) return res.status(400).send('Bad level ID');
    if (!Buffer.isBuffer(req.body) || req.body.length === 0) return res.status(400).send('Empty body');

    const ext = detectExt(req.body);
    if (!ext) return res.status(415).send('Only mp3, ogg and wav files are accepted');

    const dir = path.join(FILES_DIR, levelID);
    fs.mkdirSync(dir, { recursive: true });

    const existing = (db.mashups[levelID] || {})[req.accountID];
    if (existing && existing.ext !== ext) {
      fs.rmSync(path.join(dir, `${req.accountID}.${existing.ext}`), { force: true });
    }
    fs.writeFileSync(path.join(dir, `${req.accountID}.${ext}`), req.body);

    db.mashups[levelID] = db.mashups[levelID] || {};
    db.mashups[levelID][req.accountID] = {
      uploader: safeDecode(req.get('X-Username'), 20) || 'Unknown',
      title: safeDecode(req.get('X-Title'), 40) || 'Untitled',
      ext,
      size: req.body.length,
      downloads: existing ? existing.downloads : 0,
      createdAt: Date.now(),
    };
    saveDb();
    res.json({ ok: true });
  });

// Delete YOUR mashup for a level
app.delete('/api/mashups/:levelID', writeLimiter, auth, (req, res) => {
  const { levelID } = req.params;
  if (!isId(levelID)) return res.status(400).send('Bad level ID');
  removeMashup(levelID, req.accountID);
  res.json({ ok: true });
});

// Moderation: remove anyone's mashup (set ADMIN_KEY env var to enable)
app.delete('/api/admin/mashups/:levelID/:accountID', (req, res) => {
  if (!ADMIN_KEY || req.get('X-Admin-Key') !== ADMIN_KEY) return res.status(403).send('Forbidden');
  const { levelID, accountID } = req.params;
  if (!isId(levelID) || !isId(accountID)) return res.status(400).send('Bad ID');
  removeMashup(levelID, accountID);
  res.json({ ok: true });
});

function removeMashup(levelID, accountID) {
  const m = (db.mashups[levelID] || {})[accountID];
  if (!m) return;
  fs.rmSync(path.join(FILES_DIR, levelID, `${accountID}.${m.ext}`), { force: true });
  delete db.mashups[levelID][accountID];
  saveDb();
}

app.use((err, _req, res, _next) => {
  if (err && err.type === 'entity.too.large') return res.status(413).send('File too large (max 20 MB)');
  console.error(err);
  res.status(500).send('Server error');
});

app.listen(PORT, () => console.log(`Mashup Share server listening on :${PORT}`));
