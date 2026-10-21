'use strict';
/* ============================================================
   bank.js — банк.
   Вклад монет под процент в реальном времени (растёт и офлайн).
   Лимит вклада зависит от дохода. Часть вклада («сейф»)
   переживает престиж и возвращается стартовым капиталом.
   Прокачка ставки, лимита и сейфа — за души.
   ============================================================ */
(() => {
  const UPG = {
    rate: { name: 'Ставка', icon: '📈', max: 25, base: 10, r: 1.55, val: (l) => U.pct(0.05 + 0.01 * l, 0) + ' в час' },
    limit: { name: 'Лимит вклада', icon: '🏦', max: 30, base: 15, r: 1.5, val: (l) => U.time(60 * (1 + l)) + ' дохода' },
    safe: { name: 'Сейф', icon: '🔐', max: 20, base: 25, r: 1.7, val: (l) => U.pct(0.1 + 0.02 * l, 0) + ' вклада сохраняется' },
  };

  const M = G.mod({
    id: 'bank',
    reset: 3,
    state: () => ({ dep: 0, bal: 0, rate: 0, limit: 0, safe: 0 }),
    rate() { return (0.05 + 0.01 * G.S.bank.rate) * G.st.bank; },
    limit() { return Math.max(1000, G.income() * 60 * (1 + G.S.bank.limit)); },
    cost(k) { const d = UPG[k]; return Math.round(d.base * Math.pow(d.r, G.S.bank[k])); },
    // проценты начисляются на вклад, пока баланс не превысит вклад вдвое
    grow(sec) {
      const s = G.S.bank;
      if (s.dep <= 0) return;
      s.bal = Math.min(s.dep * 2, s.bal + s.dep * this.rate() * sec / 3600);
    },
    tick(dt) { this.grow(dt); },
    offline(sec) { this.grow(sec); },
    onReset(layer) {
      const s = G.S.bank;
      if (layer === 1 || layer === 2) {
        // сейф: часть вклада возвращается после престижа
        const keep = s.bal * (0.1 + 0.02 * s.safe);
        s.dep = 0; s.bal = 0;
        if (keep > 0) setTimeout(() => { G.add('coins', keep); G.toast(`🔐 Сейф вернул ${U.fmt(keep)} 🪙`, 'good'); }, 50);
      }
    },
    levels() { const s = G.S.bank; return s.rate + s.limit + s.safe; },
    panels: [{
      id: 'bank', group: 'more', name: 'Банк', icon: '🏦', order: 43,
      unlock: () => (G.S.stats.prestiges || 0) >= 1,
      hint: 'Первый престиж',
      render() {
        const s = G.S.bank, lim = M.limit();
        let h = `<div class="pcard"><div class="row"><span>🏦 Баланс</span><b class="hl">${U.fmt(s.bal)} 🪙</b></div>
          <div class="row dim small"><span>Вклад ${U.fmt(s.dep)} · лимит ${U.fmt(lim)}</span><span>${U.pct(M.rate(), 0)} в час</span></div>
          ${UI.bar(s.dep ? (s.bal - s.dep) / s.dep : 0, 'gold thin', s.dep ? `доход вклада ${U.pct(Math.max(0, s.bal / s.dep - 1), 0)} из 100%` : 'нет вклада')}
          <div class="btnrow"><button class="btn pri" data-a="bank.dep:0.1">+10%</button><button class="btn pri" data-a="bank.dep:0.5">+50%</button><button class="btn pri" data-a="bank.dep:1">Макс</button></div>
          <button class="btn gold wide" data-a="bank.take"${U.dis(s.bal > 0)}>💰 Снять всё (${U.fmt(s.bal)})</button>
          <div class="center dim small">Вклад растёт до ×2. При престиже ${U.pct(0.1 + 0.02 * s.safe, 0)} баланса сохраняется в сейфе.</div></div>`;
        for (const k in UPG) {
          const d = UPG[k], l = s[k], max = l >= d.max, c = M.cost(k);
          h += UI.card({ k: 'bk' + k, icon: d.icon, title: d.name, lv: `ур. ${l}`, desc: `${d.val(l)}${max ? '' : ` → <span class="hl">${d.val(l + 1)}</span>`}`,
            btn: max ? '<button class="buy max" disabled>МАКС</button>' : UI.buyC('bank.up:' + k, { souls: c }), cls: max ? 'done' : G.cur('souls') >= c ? 'can' : 'cant' });
        }
        return h;
      },
    }],
    actions: {
      dep(f) {
        const s = G.S.bank, room = Math.max(0, M.limit() - s.dep);
        const v = Math.min(room, G.cur('coins') * +f);
        if (v <= 0) { SFX.play('error'); G.toast(room <= 0 ? 'Достигнут лимит вклада' : 'Нет монет'); return; }
        G.add('coins', -v);
        s.dep += v; s.bal += v;
        SFX.play('coin');
      },
      take() {
        const s = G.S.bank;
        if (s.bal <= 0) return;
        G.add('coins', s.bal);
        G.toast(`🏦 Снято ${U.fmt(s.bal)} 🪙`, 'good');
        s.bal = 0; s.dep = 0;
        SFX.play('coin');
      },
      up(k) {
        const s = G.S.bank;
        if (s[k] >= UPG[k].max || !G.pay({ souls: M.cost(k) })) { SFX.play('error'); return; }
        s[k]++;
        SFX.play('buy');
      },
    },
  });
  G.defStat('bank', 1);
})();
