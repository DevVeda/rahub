// Página del hub: login, Web API Key y resumen del perfil. Usa lib.js; textos en web/lang.

const MEDIA = 'https://media.retroachievements.org';
const SETTINGS = 'https://retroachievements.org/settings';
let me = ''; // usuario con sesión

// ---------- Servidor (ver src/hub.c) ----------

const api = {
  state: () => getJSON('/api/state'),
  overview: refresh => getJSON('/api/overview' + (refresh ? '?refresh=1' : '')),
  login: form => postForm('/login', form),
  saveKey: form => postForm('/key', form),
  logout: () => postForm('/logout', {}),
};

// ---------- Componentes: devuelven HTML; todo dato de fuera pasa por esc() ----------

const ui = {
  error: msg => `<p class=error>${esc(msg)}</p>`,
  tile: (value, label) => `<div><b>${esc(value)}</b><small>${esc(label)}</small></div>`,
  tiles: (...tiles) => `<div class=tiles>${tiles.join('')}</div>`,
  panel: (title, body) => `<section class=panel><h2>${esc(title)}</h2>${body}</section>`,
  list: (title, items) => ui.panel(title, items.length ? items.join('') : `<p class=hint>${esc(t('empty'))}</p>`),
  // Fila de lista. opts: { right, progress (0..1), gold, big }
  item: (img, title, detail, opts = {}) =>
    `<div class="item${opts.big ? ' big' : ''}"><img src="${MEDIA}${esc(img)}" alt="" loading=lazy>` +
    `<div><b${opts.gold ? ' class=gold' : ''}>${esc(title)}</b><small>${esc(detail)}</small>` +
    (opts.progress != null ? `<div class=progress><i style="width:${Math.round(opts.progress * 100)}%"></i></div>` : '') +
    `</div>${opts.right ? `<span>${esc(opts.right)}</span>` : ''}</div>`,
};

// ---------- Secciones del resumen: cada una recibe la respuesta de RA y devuelve HTML ----------

const sections = {
  stats(s) {
    const rankLabel = s.Rank && s.TotalRanked ? t('top', Math.max(1, Math.ceil(s.Rank / s.TotalRanked * 100))) : t('rank');
    return ui.tiles(ui.tile(num(s.TotalPoints), t('hc')), ui.tile(num(s.TotalTruePoints), t('rp')),
      ui.tile(s.Rank ? '#' + num(s.Rank) : '—', rankLabel));
  },

  // Sólo si RA lo actualizó hace poco: si no, es de una partida vieja.
  nowPlaying(s) {
    const fresh = s.RichPresenceMsg && Date.now() - utc(s.RichPresenceMsgDate) < 15 * 60000;
    return fresh ? `<div class=now-playing><small>${esc(t('now'))}</small>${esc(s.RichPresenceMsg)}</div>` : '';
  },

  recentGames(s) {
    return ui.list(t('recent'), (s.RecentlyPlayed || []).map(g => {
      const a = s.Awarded?.[g.GameID] || {};
      const got = Math.max(a.NumAchieved || 0, a.NumAchievedHardcore || 0);
      const total = a.NumPossibleAchievements || g.AchievementsTotal || 0;
      return ui.item(g.ImageIcon, g.Title, `${g.ConsoleName} · ${t('ach', got, total)}`, { progress: total ? got / total : 0 });
    }));
  },

  // RecentAchievements viene agrupado { juego: { logro: … } }: se aplana y se ordena por fecha.
  latestAchievements(s) {
    const list = Object.values(s.RecentAchievements || {}).flatMap(Object.values)
      .sort((a, b) => String(b.DateAwarded).localeCompare(String(a.DateAwarded))).slice(0, 7);
    return ui.list(t('latest'), list.map(a =>
      ui.item(`/Badge/${a.BadgeName}.png`, a.Title, `${a.GameTitle} · ${ago(a.DateAwarded)}`, { right: `${a.Points} ${t('pts')}`, gold: true })));
  },

  awards(a) {
    const beaten = (a.BeatenHardcoreAwardsCount || 0) + (a.BeatenSoftcoreAwardsCount || 0);
    return ui.panel(t('awards'), ui.tiles(ui.tile(num(a.MasteryAwardsCount), t('mastered')),
      ui.tile(num(beaten), t('beaten')), ui.tile(num(a.CompletionAwardsCount), t('completed'))));
  },

  achievementOfTheWeek(w) {
    const a = w.Achievement, mine = (w.Unlocks || []).some(u => u.User === me);
    const detail = mine ? t('got') : `${w.Game?.Title} · ${w.Console?.Title}`;
    return ui.panel(t('aotw'),
      ui.item(`/Badge/${a.BadgeName}.png`, a.Title, detail, { right: `${a.Points} ${t('pts')}`, gold: true, big: true }) +
      (a.Description ? `<p class=hint>${esc(a.Description)}</p>` : ''));
  },

  wantToPlay(list) {
    return ui.list(t('wtp'), list.map(g =>
      ui.item(g.ImageIcon, g.Title, `${g.ConsoleName} · ${t('achN', g.AchievementsPublished)}`, { right: `${g.PointsTotal} ${t('pts')}` })));
  },
};

// ---------- Vistas ----------

async function start(msg = '') {
  const st = await api.state();
  if (st.error) msg = t('network');
  await useLang(st.lang);
  me = st.user || '';
  $('sub').textContent = t('sub');
  $('port').textContent = st.port ? t('port', st.port) : '';
  $('actions').innerHTML = '';
  me ? showProfile(st.key, msg) : showLogin(msg);
}

function showLogin(msg) {
  $('view').innerHTML = `<div class="card form-card">${ui.error(msg)}<form id=login-form>
    <input name=u placeholder="${esc(t('user'))}" autocomplete=username required>
    <input name=p type=password placeholder="${esc(t('pass'))}" autocomplete=current-password required>
    <button>${esc(t('login'))}</button></form></div>`;
  $('login-form').onsubmit = async e => {
    e.preventDefault();
    e.target.querySelector('button').disabled = true;
    const r = await api.login(new FormData(e.target));
    start(r.error ? t(r.error) : '');
  };
}

// Pide la Web API Key dentro de `target` (sin ella no hay resumen).
function showKeyForm(target, msg = '') {
  $(target).innerHTML = `<div class="card form-card inline">${ui.error(msg)}
    <p class=hint>${esc(t('keyHelp'))} <a href="${SETTINGS}" target=_blank rel=noopener>retroachievements.org/settings</a></p>
    <form id=key-form><input name=k placeholder="${esc(t('keyPh'))}" autocomplete=off required><button>${esc(t('save'))}</button></form></div>`;
  $('key-form').onsubmit = async e => { e.preventDefault(); await api.saveKey(new FormData(e.target)); start(); };
}

function showProfile(hasKey, msg) {
  $('actions').innerHTML = (hasKey ? `<button class=ghost id=refresh>${esc(t('refresh'))}</button>` : '') +
    `<button class=ghost id=logout>${esc(t('logout'))}</button>`;
  $('view').innerHTML = `<section class=hero>
      <img class=avatar src="${MEDIA}/UserPic/${esc(me)}.png" alt="">
      <div><small>${esc(t('as'))}</small><div class=username>${esc(me)}</div><p class=motto id=motto></p><small id=since></small></div>
      <div class=hero-side id=hero-side></div>
    </section>${ui.error(msg)}<div id=overview><p class=hint>${esc(t('loading'))}</p></div>`;
  $('logout').onclick = async () => { await api.logout(); start(); };
  if (!hasKey) return showKeyForm('overview');
  $('refresh').onclick = () => loadOverview(true);
  loadOverview(false);
}

async function loadOverview(refresh) {
  const d = await api.overview(refresh);
  if (d.error === 'badKey') return showKeyForm('overview', t('badKey'));
  const s = d.summary;
  if (s) {
    $('motto').textContent = s.Motto ? `“${s.Motto}”` : '';
    $('since').textContent = s.MemberSince ? t('since', String(s.MemberSince).slice(0, 4)) : '';
    $('hero-side').innerHTML = sections.stats(s) + sections.nowPlaying(s);
  }
  const columns = [
    [s && sections.recentGames(s), d.awards && sections.awards(d.awards)],
    [s && sections.latestAchievements(s)],
    [d.aotw?.Achievement && sections.achievementOfTheWeek(d.aotw), d.wtp?.Results && sections.wantToPlay(d.wtp.Results)],
  ];
  $('overview').innerHTML = (d.error ? ui.error(t(d.error)) : '') +
    `<div class=columns>${columns.map(c => `<div>${c.filter(Boolean).join('')}</div>`).join('')}</div>`;
}

start();
