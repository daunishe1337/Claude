'use strict';
/* ============================================================
   core.js — ядро игры.
   • реестр модулей (каждая система — отдельный модуль);
   • шина событий (тап, покупка, босс…), на неё подписываются
     достижения, квесты, сезонный пропуск;
   • валюты и платежи;
   • характеристики (статы): каждый модуль вносит свои множители,
     ядро собирает их в G.st 4 раза в секунду;
   • цены уровней (base × r^уровень) с учётом скидок и «заморозки»;
   • игровой цикл, автосохранение, сбросы престижа.
   ============================================================ */
const G = (() => {
  const G = {
    SAVE_KEY: 'tapEmpire.save.v1',
    VERSION: 1,
    mods: [],        // все модули по порядку подключения
    byId: {},        // модули по id
    S: null,         // текущее сохранение (состояние игры)
    st: {},          // вычисленные характеристики
    cps: 0,          // доход в секунду (с учётом всех множителей)
    tapVal: 1,       // доход за тап (до крита/комбо)
    dirty: true,     // нужно пересчитать характеристики
    now: Date.now(),
    freezeMap: null, // карта «замороженных» цен (бустер)
  };

  /* ---------- Модули ---------- */
  // Модуль — объект { id, state(), reset, init(), tick(dt), stats(acc),
  //   offline(sec), levels(), panels:[], actions:{}, onReset(layer) }
  G.mod = (m) => { G.mods.push(m); G.byId[m.id] = m; return m; };

  /* ---------- Шина событий ---------- */
  const listeners = {};
  G.on = (ev, fn) => { (listeners[ev] = listeners[ev] || []).push(fn); };
  G.emit = (ev, a, b, c) => {
    const l = listeners[ev];
    if (l) for (let i = 0; i < l.length; i++) l[i](a, b, c);
  };
  // короткое уведомление (показывает UI)
  G.toast = (html, cls) => G.emit('toast', html, cls);

  /* ---------- Валюты ---------- */
  // reset — на каком уровне престижа валюта обнуляется (1, 2, 3; 9 — никогда)
  G.CUR = {};
  G.defCur = (id, name, icon, reset, hud = false) => { G.CUR[id] = { id, name, icon, reset, hud }; };
  G.defCur('coins', 'Монеты', '🪙', 1);

  G.cur = (id) => G.S.cur[id] || 0;
  // начисление любой валюты
  G.add = (id, v) => {
    if (!v || v !== v) return;
    const S = G.S;
    S.cur[id] = Math.min(Number.MAX_VALUE, (S.cur[id] || 0) + v);
    if (S.cur[id] < 0) S.cur[id] = 0;
    if (v > 0) {
      S.life[id] = Math.min(Number.MAX_VALUE, (S.life[id] || 0) + v);
      if (id !== 'coins') G.emit('gain', id, v);
    }
  };
  // хватает ли валюты на стоимость {валюта: количество}
  G.can = (cost) => {
    for (const k in cost) if (G.cur(k) < cost[k] * (1 - 1e-9)) return false;
    return true;
  };
  G.pay = (cost) => {
    if (!G.can(cost)) return false;
    for (const k in cost) G.S.cur[k] = Math.max(0, G.cur(k) - cost[k]);
    G.emit('spend', cost);
    return true;
  };
  // заработок монет: учитывается в счётчиках забега и для престижа
  G.earn = (v, src) => {
    if (!(v > 0)) return;
    const c = G.S.c;
    G.add('coins', v);
    c.run = Math.min(Number.MAX_VALUE, c.run + v);
    c.asc = Math.min(Number.MAX_VALUE, c.asc + v);
    c.uni = Math.min(Number.MAX_VALUE, c.uni + v);
    G.emit('earn', v, src);
  };

  /* ---------- Счётчики статистики ---------- */
  G.count = (k, n = 1) => {
    const S = G.S;
    S.stats[k] = (S.stats[k] || 0) + n;
    S.run[k] = (S.run[k] || 0) + n;
  };
  G.best = (k, v) => {
    const S = G.S;
    if (v > (S.stats[k] || 0)) S.stats[k] = v;
    if (v > (S.run[k] || 0)) S.run[k] = v;
  };

  /* ---------- Характеристики ---------- */
  // Мультипликативные (база 1) и аддитивные (база 0 или своя).
  G.STAT = {
    all: 1,      // весь доход
    tap: 1,      // доход с тапа
    cps: 1,      // пассивный доход зданий
    cost: 1,     // цены покупок за монеты (скидки < 1)
    tapCps: 0,   // доля дохода/с, добавляемая к каждому тапу
    // временные бонусы (бустеры, навыки, события, зелья) складываются,
    // а не перемножаются: ×10 и ×100 вместе дают ×109, а не ×1000
    tapTemp: 0,
    allTemp: 0,
  };
  G.defStat = (k, base) => { G.STAT[k] = base; };

  G.recalc = () => {
    const st = Object.assign({}, G.STAT);
    const acc = {
      mul(k, v) { st[k] = (st[k] === undefined ? 1 : st[k]) * v; },
      add(k, v) { st[k] = (st[k] === undefined ? 0 : st[k]) + v; },
    };
    for (const m of G.mods) if (m.stats) m.stats(acc, st);
    // защита: испорченный стат (NaN) возвращается к базе
    for (const k in st) if (st[k] !== st[k]) { console.warn('NaN в стате', k); st[k] = G.STAT[k] !== undefined ? G.STAT[k] : 1; }
    st.tap *= 1 + st.tapTemp;
    st.all *= 1 + st.allTemp;
    G.st = st;
    const bld = G.byId.buildings, tap = G.byId.tap;
    G.cps = Math.min(Number.MAX_VALUE, (bld ? bld.rawCps() : 0) * st.cps * st.all);
    G.tapVal = Math.min(Number.MAX_VALUE, (tap ? tap.base() : 1) * st.tap * st.all + st.tapCps * G.cps);
    G.dirty = false;
  };

  /* ---------- Тап ---------- */
  // Конвейер тапа: модули добавляют хуки (крит, комбо, реликвии…)
  const tapHooks = [];
  G.tapHook = (order, fn) => { tapHooks.push({ order, fn }); tapHooks.sort((a, b) => a.order - b.order); };
  // x, y — координаты на экране (для эффектов); auto — авто-тап
  G.doTap = (x, y, auto = false, power = 1) => {
    const ctx = { x, y, auto, val: G.tapVal * power, mult: 1, crit: 0, label: '', color: '' };
    for (const h of tapHooks) h.fn(ctx);
    ctx.total = ctx.val * ctx.mult;
    G.earn(ctx.total, 'tap');
    G.count(auto ? 'autoTaps' : 'taps');
    G.emit('tap', ctx);
    return ctx;
  };

  /* ---------- Уровневые покупки ---------- */
  // Цена n уровней с уровня lvl: base·r^lvl… ; скидки только на монеты.
  // При «заморозке цен» следующие G.freezeCap уровней каждого товара стоят
  // как в момент активации, дальше цена растёт обычным образом.
  G.freezeCap = 10;
  function frozen(key, lvl) {
    const fm = G.freezeMap;
    if (!fm) return -1;
    if (fm[key] === undefined) fm[key] = lvl;
    return Math.min(fm[key], lvl);
  }
  function priceN(f, base, r, lvl, n) {
    if (f < 0) return U.geoSum(base, r, lvl, n);
    const cap = G.freezeCap;
    const a = Math.max(0, Math.min(n, f + cap - lvl));      // уровни по замороженной цене
    const L0 = Math.max(lvl, f + cap);
    return a * base * Math.pow(r, f) + U.geoSum(base, r, L0 - cap, n - a);
  }
  G.lvCost = (key, base, r, lvl, n, cur = 'coins') => {
    const disc = cur === 'coins' ? G.st.cost : 1;
    const f = cur === 'coins' ? frozen(key, lvl) : -1;
    return priceN(f, base, r, lvl, n) * disc;
  };
  G.lvMax = (key, base, r, lvl, money, cur = 'coins') => {
    let m = money / (cur === 'coins' ? G.st.cost : 1);
    const f = cur === 'coins' ? frozen(key, lvl) : -1;
    if (f < 0) return U.geoMax(base, r, lvl, m);
    const cap = G.freezeCap, p = base * Math.pow(r, f);
    const slots = Math.max(0, f + cap - lvl);
    const k = Math.floor((m / p) * (1 + 1e-9));
    if (k < slots) return k;
    m -= slots * p;
    const L0 = Math.max(lvl, f + cap);
    return slots + U.geoMax(base, r, L0 - cap, m);
  };
  // «Предложение» покупки с учётом режима ×1/×10/×100/MAX
  G.offer = (key, base, r, lvl, maxLvl = Infinity, cur = 'coins', mode) => {
    const room = maxLvl - lvl;
    if (room <= 0) return { n: 0, cost: 0, ok: false, max: true, cur };
    mode = mode || G.S.settings.buy;
    const money = G.cur(cur);
    const n = mode === 'max'
      ? Math.max(1, Math.min(room, G.lvMax(key, base, r, lvl, money, cur)))
      : Math.min(room, mode);
    const cost = G.lvCost(key, base, r, lvl, n, cur);
    return { n, cost, ok: money >= cost * (1 - 1e-9), cur };
  };
  // оплатить предложение
  G.take = (o) => {
    if (!o || !o.ok || o.max || G.cur(o.cur) < o.cost * (1 - 1e-9)) return false;
    G.S.cur[o.cur] = Math.max(0, G.cur(o.cur) - o.cost);
    G.dirty = true;
    return true;
  };

  /* ---------- Универсальные улучшения ---------- */
  // d = {name, icon, base, r, max, cur, val(lv)} ; obj — объект уровней модуля
  G.upgOffer = (mod, key, d, lv, mode) => G.offer(mod + '.' + key, d.base, d.r, lv, d.max === undefined ? Infinity : d.max, d.cur || 'coins', mode);
  // цели «ближайшей покупки» из списка улучшений модуля
  G.upgGoals = (mod, UPG, obj) => Object.keys(UPG).map((k) => ({ icon: UPG[k].icon, name: UPG[k].name, cost: (UPG[k].cur || 'coins') === 'coins' ? G.upgOffer(mod, k, UPG[k], obj[k] || 0, 1).cost : 0 })).filter((g) => g.cost > 0);
  G.buyUpg = (mod, key, d, obj) => {
    const o = G.upgOffer(mod, key, d, obj[key] || 0);
    if (!G.take(o)) { SFX.play('error'); return 0; }
    obj[key] = (obj[key] || 0) + o.n;
    G.emit('buy', 'upg', o.n, mod + '.' + key);
    SFX.play('buy');
    return o.n;
  };
  // система открыта (панель разблокирована хотя бы раз)
  G.has = (panelId) => !!(G.S && G.S.flags['p:' + panelId]);
  // опорный доход в секунду для наград событий: здания + 3 тапа/с + авто-тапы
  G.income = () => G.cps + G.tapVal * 3 + (G.byId.autotap ? G.byId.autotap.inc() : 0);

  /* ---------- Сохранение ---------- */
  G.defaults = () => {
    const now = Date.now();
    const S = {
      v: G.VERSION,
      cur: {}, life: {},
      c: { run: 0, asc: 0, uni: 0 },
      stats: {}, run: {},
      t: { created: now, save: 0, play: 0, runStart: now },
      flags: {},
      settings: { sound: true, vib: true, buy: 1, fx: true },
    };
    for (const m of G.mods) if (m.state) S[m.id] = m.state();
    return S;
  };
  G.load = () => {
    let saved = null;
    try {
      const raw = localStorage.getItem(G.SAVE_KEY);
      if (raw) saved = JSON.parse(raw);
    } catch (e) { console.warn('Ошибка чтения сохранения', e); }
    return saved ? U.merge(G.defaults(), saved) : G.defaults();
  };
  G.save = () => {
    if (!G.S || G.noSave) return;
    G.S.t.save = Date.now();
    try { localStorage.setItem(G.SAVE_KEY, JSON.stringify(G.S)); } catch (e) { console.warn('Не удалось сохранить', e); }
  };
  G.exportSave = () => { G.save(); return U.b64enc(JSON.stringify(G.S)); };
  G.importSave = (str) => {
    const data = JSON.parse(U.b64dec(str.trim()));
    if (!data || typeof data !== 'object' || !data.cur) throw new Error('bad save');
    G.noSave = true;
    localStorage.setItem(G.SAVE_KEY, JSON.stringify(data));
    location.reload();
  };
  G.hardReset = () => {
    G.noSave = true;
    localStorage.removeItem(G.SAVE_KEY);
    location.reload();
  };

  /* ---------- Сбросы (престиж) ---------- */
  // layer 1 — престиж, 2 — вознесение, 3 — перерождение вселенной
  G.resetLayer = (layer) => {
    const S = G.S, d = G.defaults();
    for (const m of G.mods) {
      if (m.state && m.reset && m.reset <= layer) S[m.id] = d[m.id];
      if (m.onReset) m.onReset(layer);
    }
    for (const id in G.CUR) if (G.CUR[id].reset <= layer) S.cur[id] = 0;
    S.c.run = 0;
    if (layer >= 2) S.c.asc = 0;
    if (layer >= 3) S.c.uni = 0;
    S.run = {};
    S.t.runStart = Date.now();
    G.freezeMap = null;
    G.dirty = true;
    G.recalc();
    G.emit('reset', layer);
  };

  /* ---------- Игровой цикл ---------- */
  G.tick = (dt) => {
    G.S.t.play += dt;
    for (const m of G.mods) if (m.tick) m.tick(dt);
    if (G.cps > 0) G.earn(G.cps * dt, 'cps');
  };
  // офлайн: модули сами решают, что начислить
  G.offline = (sec) => {
    for (const m of G.mods) if (m.offline) m.offline(sec);
    G.emit('offline', sec);
    G.dirty = true;
  };

  let lastWall = 0, statT = 0, saveT = 0, secT = 0;
  function frame() {
    const now = Date.now();
    let gap = (now - lastWall) / 1000;
    lastWall = now; G.now = now;
    if (gap > 5) { G.offline(gap); gap = 0; }      // приложение спало
    if (gap < 0) gap = 0;
    // симулируем шагами не длиннее 0.25 с
    while (gap > 0) {
      const dt = Math.min(0.25, gap);
      gap -= dt;
      statT += dt;
      if (G.dirty || statT >= 0.25) { statT = 0; G.recalc(); }
      G.tick(dt);
      saveT += dt; secT += dt;
    }
    G.emit('frame');
    if (secT >= 1) { secT = 0; G.emit('second'); }
    if (saveT >= 5) { saveT = 0; G.save(); }
    requestAnimationFrame(frame);
  }
  G.start = () => { lastWall = Date.now(); requestAnimationFrame(frame); };

  /* ---------- Запуск ---------- */
  G.boot = () => {
    G.S = G.load();
    // базовые значения статов до первого пересчёта (модули читают прошлые значения)
    G.st = Object.assign({}, G.STAT);
    for (const m of G.mods) if (m.init) m.init();
    G.recalc();
    G.recalc();
  };
  // запуск цикла (после инициализации интерфейса)
  G.run = () => {
    G.emit('boot');
    // офлайн-прогресс с момента последнего сохранения
    const gap = G.S.t.save ? (Date.now() - G.S.t.save) / 1000 : 0;
    if (gap > 10) G.offline(gap);
    G.recalc();
    G.start();
    // сохранение при сворачивании / закрытии
    document.addEventListener('visibilitychange', () => { if (document.hidden) G.save(); });
    window.addEventListener('pagehide', G.save);
    window.addEventListener('beforeunload', G.save);
  };

  return G;
})();
