'use strict';
/* ============================================================
   offline.js — офлайн-доход.
   Пока игра закрыта, здания и авто-тапер продолжают работать
   с пониженной эффективностью. Прокачиваются длительность,
   процент и шанс найти сундук. При входе — окно с итогом.
   ============================================================ */
(() => {
  const UPG = {
    hours: { name: 'Длительность', icon: '🕰️', base: 2e4, r: 2.1, max: 22, desc: 'Сколько часов копится офлайн-доход', val: (l) => (2 + l) + ' ч' },
    pct: { name: 'Эффективность', icon: '📈', base: 1.5e4, r: 1.9, max: 15, desc: 'Доля обычного дохода', val: (l) => U.pct(0.25 + 0.05 * l, 0) },
    chest: { name: 'Находки', icon: '🎁', base: 5e4, r: 2.3, max: 20, desc: 'Шанс найти сундук за каждый час', val: (l) => U.pct(0.1 + 0.03 * l, 0) },
  };

  const M = G.mod({
    id: 'offline',
    reset: 0,
    UPG,
    state: () => ({ hours: 0, pct: 0, chest: 0, last: null }),
    onReset(layer) { if (layer >= 2) { const s = G.S.offline; s.hours = s.pct = s.chest = 0; } },
    maxH() { return 2 + G.S.offline.hours + G.st.offlineH; },
    pct() { return (0.25 + 0.05 * G.S.offline.pct) * G.st.offline; },
    levels() { const s = G.S.offline; return s.hours + s.pct + s.chest; },
    goals() { return G.has('offline') ? G.upgGoals('offline', UPG, G.S.offline) : []; },

    offline(sec) {
      const t = Math.min(sec, this.maxH() * 3600);
      const auto = G.byId.autotap ? G.byId.autotap.inc() : 0;
      let coins = (G.cps + auto) * t * this.pct();
      // сундуки за полные часы
      let chests = 0;
      const p = Math.min(1, (0.1 + 0.03 * G.S.offline.chest) * G.st.drop);
      for (let h = 0; h < Math.floor(t / 3600); h++) if (Math.random() < p) chests++;
      const loot = { coins: 0, list: [] };
      for (let i = 0; i < chests; i++) {
        loot.coins += Math.max(10, G.income() * 300);
        G.emit('chest', loot);
      }
      coins += loot.coins;
      if (coins > 0) G.earn(coins, 'offline');
      G.count('offlineSec', sec);
      if (!(coins > 0)) return;
      if (sec < 60 || !UI.ready) { if (coins > 0 && UI.ready) G.toast(`💤 +${U.fmt(coins)} 🪙 за ${U.time(sec)}`); return; }
      G.S.offline.last = { sec, t, coins, chests, list: loot.list };
      UI.modal({
        title: '💤 Пока тебя не было',
        html: `<div class="center"><p>Прошло <b>${U.time(sec)}</b>${sec > t ? ` (учтено ${U.time(t)})` : ''}</p>
          <p>ты заработал</p><div class="big">🪙 ${U.fmt(coins)}</div>
          ${chests ? `<p>🎁 Найдено сундуков: <b>${chests}</b>${loot.list.length ? '<br>' + loot.list.join(' · ') : ''}</p>` : ''}
          <p class="dim">Эффективность ${U.pct(this.pct(), 0)} · лимит ${this.maxH()} ч</p></div>`,
        buttons: [{ text: 'Забрать', cls: 'pri', fn: () => { SFX.play('coin'); } }],
      });
    },

    panels: [{
      id: 'offline', group: 'upg', name: 'Офлайн', icon: '💤', order: 8,
      unlock: () => (G.S.life.coins || 0) >= 5e4,
      hint: '50K монет за всё время',
      render() {
        let h = UI.amt();
        h += UI.info(`Пока игра закрыта, вы получаете <b>${U.pct(M.pct(), 0)}</b> дохода до <b>${M.maxH()} ч</b>.<br>
          Сейчас это ≈ <b>${U.fmt((G.cps + (G.byId.autotap ? G.byId.autotap.inc() : 0)) * M.pct() * 3600)}</b> монет в час. Прокачка сохраняется после престижа.`);
        for (const k in UPG) h += UI.upg('offline', k, UPG[k], G.S.offline[k]);
        return h;
      },
    }],
    actions: { upg(k) { G.buyUpg('offline', k, UPG[k], G.S.offline); } },
  });
  G.defStat('offline', 1);
  G.defStat('offlineH', 0);
})();
