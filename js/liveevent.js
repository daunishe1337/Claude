'use strict';
/* ============================================================
   liveevent.js — ивенты.
   Каждую неделю — новое временное событие со своей валютой
   (жетоны ивента) и магазином. Жетоны падают с тапов, золотых
   объектов, боссов, сундуков и квестов. По окончании ивента
   непотраченные жетоны сгорают.
   ============================================================ */
(() => {
  G.defCur('evtok', 'Жетоны ивента', '🎟️', 9, true);
  const EVENTS = [
    { name: 'Праздник урожая', icon: '🎃', tok: '🍬', bonus: ['farm', 0.5], skin: 'pumpkin', color: '#ff8a3d' },
    { name: 'Зимняя сказка', icon: '❄️', tok: '❄️', bonus: ['boostDur', 0.25], skin: 'snow', color: '#9ff3ff' },
    { name: 'Лунный фестиваль', icon: '🏮', tok: '🏮', bonus: ['souls', 0.3], skin: 'sakura', color: '#ff4d6d' },
    { name: 'Летний карнавал', icon: '🎪', tok: '🎟️', bonus: ['evReward', 0.5], skin: 'pumpkin', color: '#ffd34d' },
  ];
  // магазин: [id, название, иконка, цена, выдача]
  const SHOP = [
    ['egg', 'Яйцо', '🥚', 40, () => G.add('eggs', 1)],
    ['eggs5', '5 яиц', '🥚', 180, () => G.add('eggs', 5)],
    ['scroll', 'Защитный свиток', '📜', 25, () => G.add('scrolls', 1)],
    ['pack', 'Пак карт', '🃏', 30, () => { if (G.S.cards) G.S.cards.packs++; }],
    ['boost', 'Сброс бустеров', '🚀', 15, () => { for (const k in G.S.boosters.ready) G.S.boosters.ready[k] = 0; }],
    ['stars', '3 звезды', '⭐', 60, () => G.add('stars', 3)],
    ['chest', 'Сундук реликвий (10%)', '🏺', 300, () => { if (Math.random() < 0.1 && G.byId.artifacts) G.byId.artifacts.giveRelic(null, 'Сундук ивента'); else { G.add('souls', 100); G.toast('🏺 В сундуке: 👻 100'); } }],
  ];
  const week = () => Math.floor((U.dayNum() + 3) / 7);

  const M = G.mod({
    id: 'liveevent',
    reset: 9,
    state: () => ({ week: -1, got: 0, bought: {} }),
    ev() { return EVENTS[week() % EVENTS.length]; },
    left() { return (7 - ((U.dayNum() + 3) % 7)) * 86400 - (Date.now() - new Date().setHours(0, 0, 0, 0)) / 1000; },
    check() {
      const s = G.S.liveevent;
      if (s.week !== week()) {
        if (s.week >= 0 && G.cur('evtok') > 0) G.toast('🎟️ Ивент завершён, жетоны сгорели');
        s.week = week(); s.got = 0; s.bought = {};
        G.S.cur.evtok = 0;
      }
    },
    give(n) {
      if (!G.has('liveevent')) return;
      this.check();
      G.add('evtok', n);
      G.S.liveevent.got += n;
    },
    stats(acc) { if (G.has('liveevent')) { const e = this.ev(); G.applyStat(acc, e.bonus[0], e.bonus[1]); } },
    init() { this.check(); },
    panels: [{
      id: 'liveevent', group: 'more', name: 'Ивент', icon: '🎪', order: 15,
      unlock: () => (G.S.stats.bossWins || 0) >= 3,
      hint: 'Победите 3 боссов',
      render() {
        M.check();
        const e = M.ev(), s = G.S.liveevent;
        let h = `<div class="pcard evb" style="--ec:${e.color}"><div class="center" style="font-size:44px">${e.icon}</div><div class="center"><b>${e.name}</b></div>
          <div class="center dim small">До конца: ${U.time(M.left())} · бонус ивента: ${G.statText(e.bonus[0], e.bonus[1])}</div>
          <div class="center">Жетоны: <b class="hl">${e.tok} ${U.fint(G.cur('evtok'))}</b> <span class="dim">(собрано ${s.got})</span></div></div>`;
        h += UI.info('Жетоны: тапы (редко), золотые объекты, боссы, сундуки и квесты. По окончании ивента непотраченные жетоны сгорают.', 'dim');
        h += UI.sec('Магазин ивента');
        const skinOwned = G.byId.cosmetics && G.byId.cosmetics.owned(e.skin);
        const skinCost = 250;
        h += UI.card({ k: 'evskin', icon: '🎨', title: 'Эксклюзивный облик', desc: skinOwned ? 'Уже получен' : 'Скин/фон этого ивента',
          btn: skinOwned ? '<button class="buy max" disabled>Есть</button>' : `<button class="buy${G.cur('evtok') >= skinCost ? ' ok' : ''}" data-a="liveevent.skin"${U.dis(G.cur('evtok') >= skinCost)}><span>${e.tok} ${skinCost}</span></button>` });
        for (const [id, name, icon, cost] of SHOP) {
          const ok = G.cur('evtok') >= cost;
          h += UI.card({ k: 'es' + id, icon, title: name, btn: `<button class="buy${ok ? ' ok' : ''}" data-a="liveevent.buy:${id}"${U.dis(ok)}><span>${e.tok} ${cost}</span></button>`, cls: ok ? 'can' : '' });
        }
        return h;
      },
    }],
    actions: {
      buy(id) {
        const it = SHOP.find((x) => x[0] === id);
        if (!it || !G.pay({ evtok: it[3] })) { SFX.play('error'); return; }
        it[4]();
        SFX.play('buy');
        G.toast(`${it[2]} ${it[1]}`, 'good');
      },
      skin() {
        const e = M.ev();
        if (!G.byId.cosmetics || G.byId.cosmetics.owned(e.skin) || !G.pay({ evtok: 250 })) { SFX.play('error'); return; }
        G.byId.cosmetics.give(e.skin);
        SFX.play('win');
      },
    },
  });
  // источники жетонов
  G.on('tap', (ctx) => { if (!ctx.auto && Math.random() < 0.004) M.give(1); });
  G.on('golden', () => M.give(3));
  G.on('bossWin', () => M.give(5));
  G.on('chest', () => M.give(2));
  G.on('questDone', (k) => M.give(k === 'd' ? 5 : 25));
  G.on('second', () => { if (G.S) M.check(); });
})();
