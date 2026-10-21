'use strict';
/* ============================================================
   autotap.js — авто-тапер.
   Покупаемые «пальцы» сами тапают объект. Прокачиваются
   скорость (тапов/с на палец) и сила (доля силы тапа).
   ============================================================ */
(() => {
  const UPG = {
    fingers: { name: 'Палец', icon: '👉', base: 2500, r: 2.4, max: 30, desc: 'Ещё один авто-палец', val: (l) => l + ' шт.' },
    spd: { name: 'Скорость пальцев', icon: '⚡', base: 8000, r: 1.9, max: 40, desc: 'Тапов в секунду каждым пальцем', val: (l) => (1 + 0.15 * l).toFixed(2) + '/с' },
    pow: { name: 'Сила пальцев', icon: '💪', base: 5000, r: 1.6, max: 150, desc: 'Доля силы тапа у авто-тапа', val: (l) => U.pct(0.25 + 0.05 * l, 0) },
  };
  let acc = 0, fxCount = 0, rendered = -1;

  const M = G.mod({
    id: 'autotap',
    reset: 1,
    UPG,
    state: () => ({ fingers: 0, spd: 0, pow: 0 }),
    // авто-тапов в секунду
    rate() {
      const s = G.S.autotap;
      return (s.fingers + G.st.fingers) * (1 + 0.15 * s.spd) * G.st.autoSpeed;
    },
    power() { return (0.25 + 0.05 * G.S.autotap.pow) * G.st.autoPow; },
    // примерный доход авто-тапов в секунду (без критов)
    inc() { return this.rate() * this.power() * G.tapVal; },
    levels() { const s = G.S.autotap; return s.fingers + s.spd + s.pow; },
    goals() { return G.has('autotap') ? G.upgGoals('autotap', UPG, G.S.autotap) : []; },
    tick(dt) {
      const r = this.rate();
      if (r <= 0) return;
      acc += r * dt;
      if (acc < 1) return;
      const n = Math.floor(acc);
      acc -= n;
      const p = this.power();
      // при высокой частоте тапы объединяются в пачки (экономия ресурсов)
      const calls = Math.min(n, 4);
      const per = n / calls;
      for (let i = 0; i < calls; i++) {
        const ctx = G.doTap(null, null, true, p * per);
        if (++fxCount % 3 === 0 && !document.hidden) autoFx(ctx);
      }
    },
    offline(sec) { /* офлайн-доход авто-тапов учитывает модуль офлайна */ void sec; },
    panels: [{
      id: 'autotap', group: 'upg', name: 'Авто-тапер', icon: '🤖', order: 4,
      unlock: () => (G.S.life.coins || 0) >= 3000,
      hint: '3K монет за всё время',
      render() {
        let h = UI.amt();
        h += UI.info(`🤖 Авто-тапов: <b>${U.fmt(M.rate())}/с</b> · сила ${U.pct(M.power(), 0)} тапа<br>Доход пальцев ≈ <b>${U.fmt(M.inc())}/с</b>`);
        for (const k in UPG) h += UI.upg('autotap', k, UPG[k], G.S.autotap[k]);
        return h;
      },
      badge: () => G.upgOffer('autotap', 'fingers', UPG.fingers, G.S.autotap.fingers, 1).ok && G.S.autotap.fingers < 3,
    }],
    actions: { upg(k) { G.buyUpg('autotap', k, UPG[k], G.S.autotap); } },
  });
  G.defStat('fingers', 0);   // бесплатные пальцы от других систем
  G.defStat('autoSpeed', 1);
  G.defStat('autoPow', 1);

  // пальцы вокруг объекта
  function renderFingers() {
    const n = Math.min(16, G.S.autotap.fingers + G.st.fingers);
    if (n === rendered) return;
    rendered = n;
    const box = document.getElementById('fingers');
    let h = '';
    for (let i = 0; i < n; i++) {
      const a = (i / n) * 360;
      h += `<span style="transform:rotate(${a}deg) translateY(calc(var(--orb) * -0.5 - 14px)) rotate(180deg)">👆</span>`;
    }
    box.innerHTML = h;
  }
  function autoFx(ctx) {
    const box = document.getElementById('fingers');
    const kids = box.children;
    if (kids.length) {
      const f = kids[(Math.random() * kids.length) | 0];
      f.classList.add('hit');
      setTimeout(() => f.classList.remove('hit'), 120);
    }
    if (UI.tab === 'tap') {
      const r = document.getElementById('orb').getBoundingClientRect();
      const x = r.left + r.width * (0.2 + Math.random() * 0.6), y = r.top + r.height * (0.25 + Math.random() * 0.5);
      FX.float(x, y, '+' + U.fmt(ctx.total), ctx.crit ? '#ffb020' : '#9fdcff', 15, 0.7);
    }
  }
  G.on('second', renderFingers);
  G.on('reset', () => { rendered = -1; renderFingers(); });
})();
