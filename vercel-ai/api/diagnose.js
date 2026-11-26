// api/diagnose.js  (Vercel serverless, Node 18+)
// Joylashuv: api/diagnose.js  va  data/qurilma_dataset_v1_1.json
// Env: GEMINI_API_KEY (majburiy), GEMINI_MODEL (ixtiyoriy), ALLOWED_ORIGIN (ixtiyoriy), DIAG_API_TOKEN (ixtiyoriy)
const KB = require('../data/qurilma_dataset_v1_1.json');
const MODEL = process.env.GEMINI_MODEL || 'gemini-3.8-flash';
const MIN_INTERVAL_MS = 30_000;
const MAX_CACHE_SIZE = 200;
const RATE_WINDOW_MS = 60_000;
const RATE_MAX_REQ = 12;

const cache = new Map(); // key -> {t, data}  (serverless'da faqat "best effort")
const rateMap = new Map(); // ip -> {t, count}

const ALLOWED_FEATURES = new Set([
  'vib_rms_m_s2', 'vib_ratio',
  'temp_motor_C', 'temp_delta_K', 'temp_rise_rate_K_per_min',
  'current_A', 'current_ratio', 'current_ratio_same_load',
  'power_W', 'power_ratio', 'sensor_ok',
]);
const ALLOWED_LOAD_MODES = new Set(['idle', 'mid', 'nominal', 'rated']);
const ALLOWED_LEVELS = new Set(['normal', 'watch', 'warning', 'alarm', 'critical']);

function sanitizeFeatures(raw) {
  const clean = {};
  for (const k of Object.keys(raw)) {
    if (!ALLOWED_FEATURES.has(k)) continue;
    const v = raw[k];
    if (k === 'sensor_ok') {
      if (typeof v === 'boolean') clean[k] = v;
    } else if (typeof v === 'number' && Number.isFinite(v)) {
      clean[k] = v;
    }
  }
  return clean;
}

function checkRateLimit(req) {
  const fwd = req.headers['x-forwarded-for'];
  const ip = (typeof fwd === 'string' ? fwd.split(',')[0].trim() : req.socket?.remoteAddress) || 'unknown';
  const now = Date.now();
  const rec = rateMap.get(ip);
  if (!rec || now - rec.t >= RATE_WINDOW_MS) {
    if (rateMap.size >= 500) rateMap.clear();
    rateMap.set(ip, { t: now, count: 1 });
    return true;
  }
  if (rec.count >= RATE_MAX_REQ) return false;
  rec.count++;
  return true;
}

function checkAuth(req) {
  const token = process.env.DIAG_API_TOKEN;
  if (!token) return true;
  const headerKey = req.headers['x-api-key'];
  const auth = req.headers['authorization'];
  const bearer = typeof auth === 'string' && auth.startsWith('Bearer ') ? auth.slice(7).trim() : '';
  return headerKey === token || bearer === token;
}

function pickContext(deviceId, ruleIds) {
  const profile = KB.device_profiles.find(p => p.device_id === deviceId);
  const rules = KB.alert_rules.filter(r => ruleIds.includes(r.id));
  const faultIds = new Set();
  rules.forEach(r => (r.fault_ranking || []).forEach(f => faultIds.add(f.fault_id)));
  const faults = KB.fault_library.filter(f => faultIds.has(f.id));
  return { profile, rules, faults };
}

function systemPrompt() {
  const g = KB.gemini_context;
  return [
    g.role_uz,
    'Qoidalar:',
    ...g.rules_uz.map(s => '- ' + s),
    'Chiqish formati (faqat JSON): ' + JSON.stringify(g.output_contract_json),
  ].join('\n');
}

function userPrompt(body, ctx, cleanFeatures) {
  const p = ctx.profile || {};
  return JSON.stringify({
    device: {
      id: body.device_id,
      type: p.type,
      sensors: p.sensors,
      features_unavailable: p.features_unavailable,
    },
    global_rules: {
      multi_signal_uz: KB.global_rules.multi_signal_uz,
      ambient_compensation_uz: KB.global_rules.ambient_compensation_uz,
    },
    features_now: cleanFeatures,
    load_mode: ALLOWED_LOAD_MODES.has(body.load_mode) ? body.load_mode : 'nominal',
    edge_level: ALLOWED_LEVELS.has(body.level) ? body.level : 'warning',
    triggered_rules: ctx.rules.map(r => ({
      id: r.id, level: r.level, message_uz: r.message_uz,
      fault_ranking: r.fault_ranking, confidence: r.confidence,
    })),
    fault_library: ctx.faults.map(f => ({
      id: f.id, name_uz: f.name_uz, symptoms: f.symptoms,
      probable_causes_uz: f.probable_causes_uz,
      consequences_uz: f.consequences_uz,
      recommended_action_uz: f.recommended_action_uz,
    })),
  });
}

function validate(out, allowedFaults) {
  if (!out || typeof out !== 'object') throw new Error('bad_model_output');
  out.ranked_causes = (out.ranked_causes || [])
    .filter(c => allowedFaults.has(c.fault_id)) // kutubxonadan tashqari sabablarni tashlab yuboramiz
    .map(c => ({ ...c, probability_pct: Math.max(0, Math.min(100, Number(c.probability_pct) || 0)) }));
  return out;
}

module.exports = async function handler(req, res) {
  res.setHeader('Access-Control-Allow-Origin', process.env.ALLOWED_ORIGIN || '*');
  res.setHeader('Access-Control-Allow-Methods', 'POST, OPTIONS');
  res.setHeader('Access-Control-Allow-Headers', 'Content-Type, x-api-key, Authorization');
  if (req.method === 'OPTIONS') return res.status(204).end();
  if (req.method !== 'POST') return res.status(405).json({ error: 'POST kerak' });

  if (!checkAuth(req)) return res.status(401).json({ error: 'unauthorized' });

  let body;
  try {
    body = typeof req.body === 'string' ? JSON.parse(req.body) : req.body;
  } catch {
    return res.status(400).json({ error: 'yaroqsiz JSON' });
  }
  if (
    !body ||
    typeof body.device_id !== 'string' ||
    !body.device_id ||
    !body.features_now ||
    typeof body.features_now !== 'object' ||
    Array.isArray(body.features_now) ||
    !Array.isArray(body.triggered_rule_ids)
  ) {
    return res.status(400).json({ error: 'device_id, features_now, triggered_rule_ids kerak' });
  }

  const ruleIds = body.triggered_rule_ids.filter(id => typeof id === 'string').slice(0, 15);

  // Qoida ishga tushmagan bo'lsa Gemini chaqirilmaydi (kvota tejaladi, uydirma kamayadi)
  if (ruleIds.length === 0) {
    return res.status(200).json({ status: 'normal', summary_uz: 'Qoidalar ishga tushmagan: holat normal.' });
  }

  const key = body.device_id + '|' + [...ruleIds].sort().join(',');
  const hit = cache.get(key);
  if (hit && Date.now() - hit.t < MIN_INTERVAL_MS) return res.status(200).json({ ...hit.data, cached: true });

  if (!checkRateLimit(req)) {
    res.setHeader('Retry-After', '60');
    return res.status(429).json({ error: 'rate_limit_exceeded' });
  }

  const ctx = pickContext(body.device_id, ruleIds);
  if (!ctx.profile) return res.status(404).json({ error: 'noma\'lum device_id' });
  if (ctx.rules.length === 0) return res.status(400).json({ error: 'noma\'lum triggered_rule_ids' });
  if (!process.env.GEMINI_API_KEY) return res.status(500).json({ error: 'GEMINI_API_KEY sozlanmagan' });

  const cleanFeatures = sanitizeFeatures(body.features_now);
  const allowedFaults = new Set(ctx.faults.map(f => f.id));

  try {
    const r = await fetch(
      `https://generativelanguage.googleapis.com/v1beta/models/${MODEL}:generateContent`,
      {
        method: 'POST',
        headers: { 'Content-Type': 'application/json', 'x-goog-api-key': process.env.GEMINI_API_KEY },
        body: JSON.stringify({
          systemInstruction: { parts: [{ text: systemPrompt() }] },
          contents: [{ role: 'user', parts: [{ text: userPrompt(body, ctx, cleanFeatures) }] }],
          generationConfig: { temperature: 0.2, responseMimeType: 'application/json' },
        }),
      }
    );
    if (!r.ok) {
      const errDetail = await r.text().catch(() => '');
      return res.status(502).json({ error: 'gemini_http_' + r.status, detail: errDetail.slice(0, 200) });
    }
    const j = await r.json();
    const text = j.candidates?.[0]?.content?.parts?.[0]?.text || '{}';
    const out = validate(JSON.parse(text), allowedFaults);
    if (cache.size >= MAX_CACHE_SIZE) {
      const now = Date.now();
      for (const [k, v] of cache) {
        if (now - v.t >= MIN_INTERVAL_MS) cache.delete(k);
      }
      if (cache.size >= MAX_CACHE_SIZE) cache.delete(cache.keys().next().value);
    }
    cache.set(key, { t: Date.now(), data: out });
    return res.status(200).json(out);
  } catch (e) {
    return res.status(500).json({ error: String(e.message || e) });
  }
};
