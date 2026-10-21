'use strict';
/* ============================================================
   crit.js — критические удары.
   Шанс крита, сила крита, шанс мегакрита (крит превращается
   в мегакрит) и сила мегакрита — каждое прокачивается отдельно.
   ============================================================ */
(() => {
  const UPG = {
    ch: { name: 'Шанс крита', icon: '🎯', base: 150, r: 1.55, max: 50, val: (l) => U.pct(0.03 + 0.005 * l) },
    mul: { name: 'Сила крита', icon: '💥', base: 400, r: 1.42, max: 200, val: (l) => U.mul(3 + 0.25 * l) },
    mch: { name: 'Шанс мегакрита', icon: '🌠', base: 2.5e4, r: 1.7, max: 30, desc: 'Шанс, что крит станет мегакритом', val: (l) => U.pct(0.02 + 0.01 * l) },
    mmul: { name: 'Сила мегакрита', icon: '☄️', base: 1e5, r: 1.5, max: 200, val: (l) => U.mul(10 + 2 * l) },
  };

  const M = G.mod({
    id: 'crit',
    reset: 1,
    UPG,
    state: () => ({ ch: 0, mul: 0, mch: 0, mmul: 0 }),
    stats(acc) {
      if (!G.has('crit')) return;
      const s = G.S.crit;
      acc.add('critCh', 0.03 + 0.005 * s.ch);
      acc.add('critMul', 3 + 0.25 * s.mul);
      acc.add('megaCh', 0.02 + 0.01 * s.mch);
      acc.add('megaMul', 10 + 2 * s.mmul);
    },
    levels() { const s = G.S.crit; return s.ch + s.mul + s.mch + s.mmul; },
    goals() { return G.has('crit') ? G.upgGoals('crit', UPG, G.S.crit) : []; },
    panels: [{
      id: 'crit', group: 'upg', name: 'Криты', icon: '🎯', order: 2,
      unlock: () => (G.S.stats.taps || 0) >= 60,
      hint: '60 тапов',
      render() {
        const st = G.st;
        let h = UI.amt();
        h += UI.info(`Шанс крита <b>${U.pct(Math.min(0.9, st.critCh))}</b> · сила <b>${U.mul(st.critMul)}</b><br>
          Мегакрит: <b>${U.pct(Math.min(1, st.megaCh))}</b> от критов · сила <b>${U.mul(st.megaMul * st.critMul)}</b>`);
        for (const k in UPG) h += UI.upg('crit', k, UPG[k], G.S.crit[k]);
        return h;
      },
      badge: () => Object.keys(UPG).some((k) => G.S.crit[k] === 0 && G.upgOffer('crit', k, UPG[k], 0).ok),
    }],
    actions: { upg(k) { G.buyUpg('crit', k, UPG[k], G.S.crit); } },
  });
  void M;
  G.defStat('critCh', 0);
  G.defStat('critMul', 0);
  G.defStat('megaCh', 0);
  G.defStat('megaMul', 0);

  // хук тапа: бросок крита
  G.tapHook(20, (ctx) => {
    const st = G.st;
    if (!(st.critCh > 0) || Math.random() >= Math.min(0.9, st.critCh)) return;
    ctx.crit = 1;
    ctx.mult *= st.critMul;
    ctx.label = 'КРИТ!';
    ctx.color = '#ffb020';
    G.count('crits');
    if (Math.random() < Math.min(1, st.megaCh)) {
      ctx.crit = 2;
      ctx.mult *= st.megaMul;
      ctx.label = 'МЕГАКРИТ!';
      ctx.color = '#ff3df2';
      G.count('megas');
    }
  });
})();
