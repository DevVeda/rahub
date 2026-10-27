// Utilidades compartidas por las páginas del hub y de la app de prueba (scripts clásicos, sin módulos:
// el navegador de la PS5 es un WebKit viejo).

const $ = id => document.getElementById(id);

// Todo texto que viene de fuera pasa por aquí antes de ir a innerHTML.
const esc = s => String(s ?? '').replace(/[&<>"']/g, c => '&#' + c.charCodeAt(0) + ';');

// ---------- i18n: textos en /lang/<código>.json (web/lang); el idioma lo elige el servidor ----------

const i18n = { lang: 'en', dict: {} };

// Carga el idioma encima del inglés: lo que le falte a una traducción sale en inglés.
async function useLang(lang = 'en') {
  const [en, own] = await Promise.all([getJSON('/lang/en.json'), lang === 'en' ? {} : getJSON(`/lang/${lang}.json`)]);
  i18n.lang = lang;
  i18n.dict = Object.assign({}, en.error ? {} : en, own.error ? {} : own);
  document.documentElement.lang = lang;
}

// t('ach', 3, 10) con ach: '{0} / {1} achievements'. Clave desconocida = se devuelve tal cual (p. ej. un error de RA).
const t = (key, ...args) => String(i18n.dict[key] ?? key).replace(/\{(\d)\}/g, (_, i) => args[i]);

const num = n => Number(n || 0).toLocaleString(i18n.lang);

// RA da fechas UTC sin zona: "2024-01-31 18:05:00".
const utc = d => Date.parse(String(d).replace(' ', 'T') + 'Z');

function ago(d) {
  const s = (utc(d) - Date.now()) / 1000;
  if (!isFinite(s)) return '';
  if (typeof Intl == 'undefined' || !Intl.RelativeTimeFormat) return new Date(utc(d)).toLocaleDateString(i18n.lang);
  const f = new Intl.RelativeTimeFormat(i18n.lang, { numeric: 'auto' });
  for (const [unit, secs] of [['year', 31536000], ['month', 2592000], ['day', 86400], ['hour', 3600], ['minute', 60]])
    if (Math.abs(s) >= secs) return f.format(Math.round(s / secs), unit);
  return f.format(0, 'minute');
}

// ---------- Llamadas al servidor: nunca lanzan, devuelven { error: 'network' } ----------

async function getJSON(path) {
  try { return await (await fetch(path)).json(); } catch { return { error: 'network' }; }
}

// La cabecera X-RAHub la exige el servidor en los POST (protección CSRF).
async function postForm(path, data) {
  try {
    const r = await fetch(path, { method: 'POST', headers: { 'X-RAHub': '1' }, body: new URLSearchParams(data) });
    return await r.json();
  } catch { return { error: 'network' }; }
}
