// Prueba las páginas sin navegador (DOM y servidor falsos), en inglés o en español.
//   node tests/page.test.mjs en|es   (o make test)
import fs from 'fs';

const lang = process.argv[2];
const now = new Date().toISOString().slice(0, 19).replace('T', ' ');

// Los archivos de idioma de verdad, como los sirve el ELF.
const LANGS = Object.fromEntries(fs.readdirSync('web/lang').map(f => [`/lang/${f}`, JSON.parse(fs.readFileSync(`web/lang/${f}`, 'utf8'))]));

// Carga lib.js + la página con un DOM mínimo (getElementById) y fetch contestando `responses`.
async function load(page, responses) {
  responses = { ...LANGS, ...responses };
  const els = {};
  const node = () => ({ innerHTML: '', textContent: '', className: '', appendChild: c => c });
  globalThis.document = { getElementById: id => els[id] ??= node(), documentElement: {}, createElement: node };
  globalThis.fetch = async url => ({ json: async () => structuredClone(responses[url.split('?')[0]] ?? { error: 'no existe' }) }); // como el 404 del servidor
  const code = ['web/lib.js', page].map(f => fs.readFileSync(f, 'utf8')).join('\n');
  (0, eval)(code.replace(/\nconst /g, '\nvar ')); // global como en el navegador; const → var para recargar
  await new Promise(r => setTimeout(r, 20));
  return els;
}

function expect(ok, what) {
  if (!ok) throw new Error(`${lang}: ${what}`);
}

// ---------- Hub ----------
const hub = await load('web/hub.js', {
  '/api/state': { lang, port: 8045, user: 'Scott', key: true, ps5user: 'Ana' },
  '/api/overview': {
    error: '',
    summary: {
      Motto: 'hi <b>', MemberSince: '2013-05-01 00:00:00', TotalPoints: 12480, TotalTruePoints: 31902, Rank: 4616, TotalRanked: 45654,
      RichPresenceMsg: 'Green Hill <x>', RichPresenceMsgDate: now,
      RecentlyPlayed: [{ GameID: 1, Title: 'Sonic', ConsoleName: 'Genesis', ImageIcon: '/Images/1.png', AchievementsTotal: 25 }],
      Awarded: { 1: { NumAchieved: 18, NumAchievedHardcore: 18, NumPossibleAchievements: 25 } },
      RecentAchievements: { 1: {
        9: { Title: 'Ring', GameTitle: 'Sonic', BadgeName: '108302', Points: 10, DateAwarded: '2020-01-01 00:00:00' },
        8: { Title: 'Newer', GameTitle: 'Sonic', BadgeName: '1', Points: 5, DateAwarded: now } } },
    },
    awards: { MasteryAwardsCount: 3, BeatenHardcoreAwardsCount: 2, BeatenSoftcoreAwardsCount: 1, CompletionAwardsCount: 4 },
    aotw: { Achievement: { Title: 'AOTW', BadgeName: '5', Points: 25 }, Game: { Title: 'Mario' }, Console: { Title: 'NES' }, Unlocks: [{ User: 'Scott' }] },
    wtp: { Results: [{ Title: 'Zelda', ConsoleName: 'NES', ImageIcon: '/Images/2.png', AchievementsPublished: 40, PointsTotal: 400 }] },
  },
});
const html = hub.overview.innerHTML + hub['hero-side'].innerHTML;
const must = lang === 'es'
  ? ['Jugando ahora', 'Últimos logros', 'Desbloqueado ✓', 'Dominados', 'Quiero jugar', '18 / 25 logros', 'Sirviendo en puerto: 8045']
  : ['Playing now', 'Latest achievements', 'Unlocked ✓', 'Mastered', 'Want to play', '18 / 25 achievements', 'Serving port: 8045'];
for (const m of must) expect((html + hub.port.textContent).includes(m), `falta "${m}"`);
expect(!html.includes('<x>') && hub.motto.textContent === '“hi <b>”', 'texto de RA sin escapar');
expect(html.indexOf('Newer') < html.indexOf('Ring'), 'logros fuera de orden');
expect(hub.actions.innerHTML.includes('id=logout'), 'falta cerrar sesión');
expect(hub.sub.textContent.includes('Ana'), 'falta el perfil de PS5');

// ---------- App de prueba ----------
const on = await load('web/test.js', { '/api/state': { lang, user: 'Scott' } });
expect(on.status.textContent === (lang === 'es' ? 'Loggeado: ' : 'Logged in: '), 'test: con sesión');
const off = await load('web/test.js', { '/api/state': { lang, user: '' } });
expect(off.status.textContent === (lang === 'es' ? 'No loggeado' : 'Not logged in'), 'test: sin sesión');

// ---------- Idioma sin archivo: inglés ----------
const fr = await load('web/test.js', { '/api/state': { lang: 'fr', user: '' } });
expect(fr.status.textContent === 'Not logged in', 'idioma sin archivo no cae a inglés');

console.log(lang, 'ok');
