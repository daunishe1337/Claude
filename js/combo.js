'use strict';
/* ============================================================
   combo.js — комбо.
   Каждый ручной тап увеличивает счётчик; множитель растёт до
   максимума (длина × сила за тап). Без тапов комбо угасает.
   Бонусы за 100 / 500 / 1000 тапов подряд (и каждую 1000 далее).
   ============================================================ */
(() => {
  const UPG = {
    len: { name: 'Длина комбо', icon: '📏', base: 600, r: 1.8, max: 60, desc: 'Сколько тапов подряд учитывается', val: (l) => (25 + 5 * l) + ' тапов' },
    pow: { name: 'Макс. множитель', icon: '🔥', base: 900, r: 1.85, max: 60, desc: 'Множитель на полной длине', val: (l) => U.mul(1 + M.lenOf(G.S.combo.len) * M.powOf(l)) },
    decay: { name: 'Время угасания', icon: '⏳', base: 1500, r: 2.5, max: 30, val: (l) => (1.2 + 0.1 * l).toFixed(1) + ' с' },
    rew: { name: 'Награды за серии', icon: '🎁', base: 5000, r: 2.2, max: 40, desc: 'Бонус за 100/500/1000 тапов подряд', val: (l) => '+' + U.pct(0.25 * l, 0) },
  };
  // награды за серии: порог -> секунды дохода
  const STREAKS = [[100, 10], [500, 30], [1000, 90]];
  let count = 0, timer = 0;

  const M = G.mod({
    id: 'combo',
    reset: 1,
    UPG,
    state: () => ({ len: 0, pow: 0, decay: 0, rew: 0 }),
    lenOf: (l) => 25 + 5 * l + G.st.comboLen,
    powOf: (l) => (0.01 + 0.0003 * l) * (G.st.comboPow || 1),
    decayT: () => 1.2 + 0.1 * G.S.combo.decay + G.st.comboDecay,
    count: () => count,
    // текущий множитель комбо
    mult() {
      const s = G.S.combo;
      return 1 + Math.min(count, this.lenOf(s.len)) * this.powOf(s.pow);
    },
    maxMult() { const s = G.S.combo; return 1 + this.lenOf(s.len) * this.powOf(s.pow); },
    levels() { const s = G.S.combo; return s.len + s.pow + s.decay + s.rew; },
    goals() { return G.has('combo') ? G.upgGoals('combo', UPG, G.S.combo) : []; },
    tick(dt) {
      if (count > 0) {
        // реликвия/таланты могут замедлять угасание
        timer -= dt;
        if (timer <= 0) { count = G.st.comboKeep > 0 ? Math.floor(count * G.st.comboKeep) : 0; timer = count ? this.decayT() : 0; }
      }
    },
    panels: [{
      id: 'combo', group: 'upg', name: 'Комбо', icon: '🔥', order: 3,
      unlock: () => (G.S.stats.taps || 0) >= 200,
      hint: '200 тапов',
      render() {
        let h = UI.amt();
        h += UI.info(`Макс. множитель <b>${U.mul(M.maxMult())}</b> на ${M.lenOf(G.S.combo.len)} тапах · угасание ${M.decayT().toFixed(1)} с<br>
          Лучшее комбо: <b>${U.fint(G.S.stats.maxCombo || 0)}</b>. Серии 100/500/1000 тапов дают ${STREAKS.map((s) => s[1] >= 60 ? s[1] / 60 + ' мин' : s[1] + ' с').join(' / ')} дохода.`);
        for (const k in UPG) h += UI.upg('combo', k, UPG[k], G.S.combo[k]);
        return h;
      },
      badge: () => Object.keys(UPG).some((k) => G.S.combo[k] === 0 && G.upgOffer('combo', k, UPG[k], 0).ok),
    }],
    actions: { upg(k) { G.buyUpg('combo', k, UPG[k], G.S.combo); } },
  });
  G.defStat('comboLen', 0);
  G.defStat('comboPow', 1);
  G.defStat('comboDecay', 0);
  G.defStat('comboKeep', 0);   // доля комбо, сохраняемая при угасании
  G.defStat('comboReward', 1);

  // хук тапа: рост комбо и множитель
  G.tapHook(10, (ctx) => {
    if (!G.has('combo')) return;
    if (ctx.auto && !(G.st.autoCombo > 0)) return;
    count++;
    timer = M.decayT();
    ctx.mult *= M.mult();
    G.best('maxCombo', count);
    // награды за серии
    let reward = 0;
    for (const [n, sec] of STREAKS) if (count === n) reward = sec;
    if (count > 1000 && count % 1000 === 0) reward = 90;
    if (reward) {
      const v = G.income() * reward * (1 + 0.25 * G.S.combo.rew) * G.st.comboReward;
      G.earn(v, 'combo');
      G.count('streaks');
      G.toast(`🔥 Серия ${count} тапов! +${U.fmt(v)} 🪙`, 'good');
      SFX.play('coin');
      G.emit('streak', count);
    }
  });
  G.defStat('autoCombo', 0);

  // виджет комбо под объектом тапа
  UI.view('comboBox', 1, () => {
    if (!G.has('combo') || count < 2) return '';
    const m = M.mult(), full = M.lenOf(G.S.combo.len);
    const k = U.clamp(timer / M.decayT(), 0, 1);
    return `<div class="combo${count >= full ? ' full' : ''}"><div class="cmb-t"><b>${U.mul(m)}</b> КОМБО <span>${count}</span></div>
      <div class="cmb-bar"><i style="width:${(k * 100).toFixed(0)}%"></i></div></div>`;
  });
})();
