'use strict';
/* ============================================================
   artifacts.js — артефакты и реликвии.
   • 20 артефактов покупаются за души (случайный из неоткрытых),
     у каждого уникальный эффект и уровни до 100;
   • реликвии — очень редкая добыча (боссы, сундуки, башня,
     подземелья) с уникальными механиками, например «каждый
     100-й тап ×1000».
   ============================================================ */
(() => {
  // [id, название, иконка, стат, значение за уровень, тип роста цены]
  const ART = [
    ['phoenix', 'Перо Феникса', '🪶', 'all', 0.1],
    ['titan', 'Кулак титана', '🤜', 'tap', 0.15],
    ['gearT', 'Шестерня времени', '⚙️', 'cps', 0.12],
    ['crown', 'Корона королей', '👑', 'crystal', 0.03],
    ['hglass', 'Песочные часы', '⏳', 'offline', 0.04],
    ['clover', 'Четырёхлистник', '🍀', 'critCh', 0.002],
    ['hammer', 'Молот грома', '🔨', 'critMul', 0.15],
    ['ring', 'Кольцо душ', '💍', 'souls', 0.05],
    ['compass', 'Компас странника', '🧭', 'evFreq', 0.03],
    ['greed', 'Сундук жадности', '🧰', 'evReward', 0.08],
    ['mask', 'Маска охотника', '🎭', 'bossDmg', 0.08],
    ['scroll', 'Свиток мудрости', '📜', 'xp', 0.05],
    ['orb', 'Хрустальный шар', '🔮', 'skillPow', 0.04],
    ['battery', 'Вечная батарейка', '🔋', 'boostDur', 0.03],
    ['map', 'Звёздная карта', '🗺️', 'stageReq', 0.008],
    ['paw', 'Лапа удачи', '🐾', 'pet', 0.04],
    ['pick', 'Кирка гномов', '⛏️', 'mine', 0.05],
    ['can', 'Лейка друида', '🚿', 'farmSpeed', 0.03],
    ['banner', 'Знамя легиона', '🚩', 'combat', 0.05],
    ['sphere', 'Сфера вечности', '🔵', 'metaK', 0.01],
  ].map(([id, name, icon, stat, v], i) => ({ id, i, name, icon, stat, v }));
  const BYA = {};
  ART.forEach((a) => (BYA[a.id] = a));
  const MAXL = 100;

  // реликвии: уникальные механики
  const REL = [
    { id: 'hundred', name: 'Сотый тап', icon: '💯', desc: 'Каждый 100-й тап приносит ×1000' },
    { id: 'eternity', name: 'Часы вечности', icon: '⌛', desc: 'Офлайн: +12 ч к лимиту и доход ×1.5' },
    { id: 'heart', name: 'Сердце дракона', icon: '❤️‍🔥', desc: 'Комбо при угасании сохраняет половину' },
    { id: 'engine', name: 'Вечный двигатель', icon: '♾️', desc: 'Авто-тапы тоже растят комбо' },
    { id: 'magnet', name: 'Магнит', icon: '🧲', desc: 'Золотые объекты собираются сами' },
    { id: 'mirror', name: 'Зеркало судьбы', icon: '🪞', desc: '25% шанс, что навык не потратит ману' },
    { id: 'lucky', name: 'Счастливая монета', icon: '🪙', desc: '1% шанс, что тап принесёт ×777' },
    { id: 'tyrant', name: 'Корона тирана', icon: '🫅', desc: 'У боссов на 50% меньше HP' },
    { id: 'star', name: 'Путеводная звезда', icon: '⭐', desc: 'Каждый новый этап дарит 2 с дохода' },
    { id: 'horn', name: 'Рог изобилия', icon: '📯', desc: 'Сундуки дают ×5 монет' },
    { id: 'chronos', name: 'Часы Хроноса', icon: '🕰️', desc: 'Перезарядка бустеров и навыков −30%' },
    { id: 'grail', name: 'Кубок бессмертия', icon: '🏆', desc: 'Время боя с боссом ×2' },
  ];
  const BYR = {};
  REL.forEach((r) => (BYR[r.id] = r));
  let tapN = 0;

  const M = G.mod({
    id: 'artifacts',
    reset: 3,
    ART, REL,
    state: () => ({ lv: {}, rel: {} }),
    owned() { return Object.keys(G.S.artifacts.lv).length; },
    nextCost() { return Math.round(5 * Math.pow(1.8, this.owned())); },
    upCost(a) { const l = G.S.artifacts.lv[a.id] || 0; return Math.ceil((1 + a.i * 0.15) * Math.pow(1.13, l)); },
    has: (id) => !!G.S.artifacts.rel[id],
    stats(acc) {
      const s = G.S.artifacts;
      for (const id in s.lv) { const a = BYA[id]; if (a && s.lv[id]) G.applyStat(acc, a.stat, a.v * s.lv[id]); }
      if (this.has('eternity')) { acc.add('offlineH', 12); acc.mul('offline', 1.5); }
      if (this.has('heart')) acc.add('comboKeep', 0.5);
      if (this.has('engine')) acc.add('autoCombo', 1);
      if (this.has('tyrant')) acc.mul('bossHp', 0.5);
      if (this.has('chronos')) { acc.mul('boostCd', 0.7); acc.mul('skillCd', 0.7); }
      if (this.has('grail')) acc.add('bossTime', 30);
    },
    levels() { const s = G.S.artifacts; let n = Object.keys(s.rel).length * 10; for (const id in s.lv) n += s.lv[id]; return n; },
    // выдать реликвию (повтор → души)
    giveRelic(id, src) {
      const s = G.S.artifacts;
      id = id || U.pick(REL.filter((r) => !s.rel[r.id]).map((r) => r.id).concat(['dupe']));
      if (id === 'dupe' || s.rel[id]) { G.add('souls', 100); G.toast('🏺 Повторная реликвия → +100 👻'); return; }
      s.rel[id] = Date.now();
      const r = BYR[id];
      SFX.play('win');
      FX.flash('rgba(255,211,77,.5)');
      UI.modal({ title: '🏺 Реликвия!', html: `<div class="center"><div style="font-size:64px">${r.icon}</div><div class="big" style="font-size:22px">${r.name}</div><p>${r.desc}</p><p class="dim">${src || ''}</p></div>`, buttons: [{ text: 'Невероятно!', cls: 'gold' }] });
      G.count('relics');
      G.dirty = true;
    },
    panels: [
      {
        id: 'artifacts', group: 'hero', name: 'Артефакты', icon: '🏺', order: 6,
        unlock: () => (G.S.life.souls || 0) >= 20,
        hint: '20 душ за всё время',
        render() {
          const s = G.S.artifacts, n = M.owned(), c = M.nextCost();
          let h = `<div class="pcard"><div class="row"><span>👻 Души: <b>${U.fint(G.cur('souls'))}</b></span><span>Открыто ${n}/${ART.length}</span></div>
            ${n < ART.length ? `<button class="btn gold wide" data-a="artifacts.dig"${U.dis(G.cur('souls') >= c)}>🏺 Найти артефакт · 👻 ${U.fmt(c)}</button>` : '<div class="center good">Все артефакты найдены!</div>'}
            <div class="center dim small">Непотраченные души дают +1% к доходу каждая — тратьте с умом</div></div>`;
          for (const a of ART) {
            const l = s.lv[a.id];
            if (l === undefined) { h += UI.card({ k: 'a' + a.id, icon: '❔', title: '???', desc: 'Не найден', cls: 'cant' }); continue; }
            const uc = M.upCost(a), ok = l < MAXL && G.cur('souls') >= uc;
            h += UI.card({
              k: 'a' + a.id, icon: a.icon, title: a.name, lv: `ур. ${l}/${MAXL}`,
              desc: `${G.statText(a.stat, a.v * l)}${l < MAXL ? ` → <span class="hl">${G.statText(a.stat, a.v * (l + 1))}</span>` : ''}`,
              btn: l >= MAXL ? '<button class="buy max" disabled>МАКС</button>' : `<button class="buy${ok ? ' ok' : ''}" data-a="artifacts.up:${a.id}"${U.dis(ok)}><span>👻 ${U.fmt(uc)}</span></button>`,
              cls: ok ? 'can' : '',
            });
          }
          return h;
        },
        badge: () => M.owned() < ART.length && G.cur('souls') >= M.nextCost() * 3,
      },
      {
        id: 'relics', group: 'hero', name: 'Реликвии', icon: '💠', order: 7,
        unlock: () => Object.keys(G.S.artifacts.rel).length > 0,
        hint: 'Найдите первую реликвию (боссы, сундуки, башня)',
        render() {
          const s = G.S.artifacts;
          let h = UI.info(`Реликвии — очень редкая добыча с уникальными механиками. Найдено <b>${Object.keys(s.rel).length}/${REL.length}</b>.`);
          for (const r of REL) {
            const own = !!s.rel[r.id];
            h += UI.card({ k: 'r' + r.id, icon: own ? r.icon : '❔', title: own ? r.name : '???', desc: own ? r.desc : 'Не найдена', cls: own ? 'done' : 'cant' });
          }
          return h;
        },
      },
    ],
    actions: {
      dig() {
        const s = G.S.artifacts, c = M.nextCost();
        const left = ART.filter((a) => s.lv[a.id] === undefined);
        if (!left.length || !G.pay({ souls: c })) { SFX.play('error'); return; }
        const a = U.pick(left);
        s.lv[a.id] = 1;
        SFX.play('win');
        G.toast(`🏺 Артефакт: <b>${a.icon} ${a.name}</b>`, 'ach');
      },
      up(id) {
        const s = G.S.artifacts, a = BYA[id];
        if (!a || s.lv[id] === undefined || s.lv[id] >= MAXL) return;
        if (!G.pay({ souls: M.upCost(a) })) { SFX.play('error'); return; }
        s.lv[id]++;
        SFX.play('buy');
      },
    },
  });

  /* ---------- Механики реликвий ---------- */
  G.tapHook(30, (ctx) => {
    if (M.has('hundred') && ++tapN % 100 === 0) { ctx.mult *= 1000; ctx.label = '💯'; ctx.color = '#ffd34d'; ctx.crit = Math.max(ctx.crit, 2); }
    if (M.has('lucky') && Math.random() < 0.01) { ctx.mult *= 777; ctx.label = '🪙777'; ctx.color = '#ffd34d'; ctx.crit = Math.max(ctx.crit, 2); }
  });
  G.on('evSpawn', (t) => {
    if (t === 'golden' && M.has('magnet')) {
      setTimeout(() => { const el = document.querySelector('.ev-golden'); if (el) el.dispatchEvent(new PointerEvent('pointerdown', { clientX: innerWidth / 2, clientY: innerHeight / 2 })); }, 600);
    }
  });
  G.on('chest', (loot) => { if (M.has('horn')) loot.coins *= 5; });
  let lastStage = 0;
  G.on('second', () => {
    const st = G.S.stage.stage;
    if (M.has('star') && st > lastStage && lastStage) G.earn(G.income() * 2 * (st - lastStage), 'event');
    lastStage = st;
  });
  // шанс реликвии: боссы 0.5% (первая победа 3%), сундуки 0.3%
  G.on('bossWin', (n, first) => { if (Math.random() < (first ? 0.03 : 0.005) * G.st.drop) M.giveRelic(null, `Добыча босса этапа ${n}`); });
  G.on('chest', () => { if (Math.random() < 0.003 * G.st.drop) setTimeout(() => M.giveRelic(null, 'Найдена в сундуке'), 300); });

  G.achCat('relic', 'Сокровища', '🏺', 'Души +50%', (a) => a.mul('souls', 1.5));
  G.achTier('relic', 'ar', ['Археолог', 'Хранитель', 'Музей'], () => M.owned(), [1, 10, 20], (t) => `Найти ${t} артефактов`);
  G.achTier('relic', 'rl', ['Реликвия', 'Сокровищница', 'Легендарная коллекция'], () => Object.keys(G.S.artifacts.rel).length, [1, 4, 12], (t) => `Найти ${t} реликвий`);
})();
