'use strict';
/* ============================================================
   buildings.js — здания (автодоход) и Мастерство.
   • 20 зданий от лимонадной лавки до мультивселенной;
   • цена base × 1.15^уровень;
   • множитель ×2 на 25/50/100/200/500 уровнях;
   • 3 личных улучшения у каждого (×2, ×3, ×5);
   • синергия: каждые 10 уровней здания дают +1% соседям;
   • Мастерство: бонус от суммарного уровня всех зданий.
   ============================================================ */
(() => {
  const R = 1.15;
  const MILESTONES = [25, 50, 100, 200, 500];
  // [название, иконка, базовая цена, доход/с за уровень]
  const DEFS = [
    ['Лимонадная лавка', '🍋', 15, 0.1],
    ['Пекарня', '🥐', 100, 1],
    ['Мастерская', '🔨', 1100, 8],
    ['Лесопилка', '🪚', 12e3, 47],
    ['Фабрика', '🏭', 130e3, 260],
    ['Торговый порт', '⚓', 1.4e6, 1400],
    ['Небоскрёб', '🏙️', 20e6, 7800],
    ['Лаборатория', '🧪', 330e6, 44e3],
    ['Электростанция', '⚡', 5.1e9, 260e3],
    ['Корпорация', '🏢', 75e9, 1.6e6],
    ['Завод роботов', '🤖', 1e12, 10e6],
    ['Портал', '🌀', 14e12, 65e6],
    ['Машина времени', '⏳', 170e12, 430e6],
    ['Орбитальная станция', '🛰️', 2.1e15, 2.9e9],
    ['Лунная база', '🌙', 26e15, 21e9],
    ['Колония на Марсе', '🔴', 310e15, 150e9],
    ['Звёздная кузня', '☄️', 7.1e18, 1.1e12],
    ['Сфера Дайсона', '🌞', 120e18, 8.3e12],
    ['Генератор чёрных дыр', '🕳️', 1.9e21, 64e12],
    ['Мультивселенная', '🌌', 54e21, 500e12],
  ];
  const B = DEFS.map(([name, icon, cost, cps], i) => ({ i, name, icon, cost, cps }));
  // личные улучшения: множитель, требуемый уровень, цена в базовых ценах здания
  const UPS = [
    { m: 2, req: 10, k: 40, name: 'Модернизация' },
    { m: 3, req: 40, k: 4000, name: 'Автоматизация' },
    { m: 5, req: 100, k: 2e7, name: 'Совершенство' },
  ];
  const msMult = (lv) => { let m = 1; for (const x of MILESTONES) if (lv >= x) m *= 2; return m; };
  const nextMs = (lv) => MILESTONES.find((x) => x > lv);

  const M = G.mod({
    id: 'buildings',
    reset: 1,
    B, UPS,
    state: () => ({ lv: B.map(() => 0), up: B.map(() => [0, 0, 0]) }),

    total() { return G.S.buildings.lv.reduce((a, b) => a + b, 0); },
    // множитель конкретного здания (вехи, улучшения, синергия)
    mult(i) {
      const s = G.S.buildings, lv = s.lv;
      let m = msMult(lv[i]);
      UPS.forEach((u, j) => { if (s.up[i][j]) m *= u.m; });
      const syn = (i > 0 ? Math.floor(lv[i - 1] / 10) : 0) + (i < B.length - 1 ? Math.floor(lv[i + 1] / 10) : 0);
      m *= 1 + 0.01 * syn * G.st.synergy;
      return m * G.st.bldMult;
    },
    // доход здания в секунду до глобальных множителей
    one(i) { return B[i].cps * G.S.buildings.lv[i] * this.mult(i); },
    rawCps() {
      let s = 0;
      for (let i = 0; i < B.length; i++) if (G.S.buildings.lv[i]) s += this.one(i);
      return s;
    },
    // Мастерство: каждые 10 суммарных уровней = 1 уровень, +4% к доходу зданий
    mastery() { return Math.floor(this.total() / 10); },
    stats(acc) { acc.mul('cps', 1 + 0.04 * this.mastery() * (G.st.masteryK || 1)); },
    levels() { const s = G.S.buildings; return this.total() + s.up.reduce((a, u) => a + u[0] + u[1] + u[2], 0); },

    shown() {
      let last = -1;
      G.S.buildings.lv.forEach((v, i) => { if (v > 0) last = i; });
      return Math.min(B.length, last + 2);
    },
    offerOf(i) { return G.offer('b' + i, B[i].cost, R, G.S.buildings.lv[i]); },
    upCost(i, j) { return B[i].cost * UPS[j].k * G.st.cost; },
    goals() {
      const out = [];
      const n = this.shown();
      for (let i = 0; i < n; i++) {
        out.push({ icon: B[i].icon, name: B[i].name, cost: G.lvCost('b' + i, B[i].cost, R, G.S.buildings.lv[i], 1) });
        UPS.forEach((u, j) => { if (!G.S.buildings.up[i][j] && G.S.buildings.lv[i] >= u.req) out.push({ icon: B[i].icon, name: `${u.name} ${U.mul(u.m)}`, cost: this.upCost(i, j) }); });
      }
      return out;
    },
    farGoal() {
      const n = this.shown();
      if (n < B.length && G.S.buildings.lv[n - 1] === 0) {
        const b = B[n - 1], need = b.cost * G.st.cost - G.cur('coins');
        const inc = UI.income();
        return { text: `Открыть: ${b.icon} ${b.name}`, eta: need > 0 && inc > 0 ? '≈ ' + U.time(need / inc) : '', prio: 1 };
      }
      return null;
    },

    panels: [{
      id: 'bld', group: 'bld', name: 'Здания', icon: '🏭', order: 1,
      render() {
        const s = G.S.buildings;
        const ml = M.mastery(), tot = M.total();
        let h = UI.amt();
        h += `<div class="info"><div class="row"><span>⚡ Доход: <b>${U.fmt(G.cps)}/с</b></span><span>🏅 Мастерство ${ml}</span></div>
          ${UI.bar((tot % 10) / 10, 'thin', `Σ уровней ${tot} · +${U.pct(0.04 * ml * (G.st.masteryK || 1), 0)} к зданиям`)}</div>`;
        const n = M.shown();
        const total = Math.max(1e-9, M.rawCps());
        for (let i = 0; i < n; i++) {
          const b = B[i], lv = s.lv[i];
          const o = M.offerOf(i);
          const per = b.cps * M.mult(i) * G.st.cps * G.st.all;
          const nx = nextMs(lv);
          let ups = '';
          UPS.forEach((u, j) => {
            if (s.up[i][j]) ups += `<span class="mini done">✔ ${U.mul(u.m)}</span>`;
            else if (lv >= u.req) {
              const c = M.upCost(i, j), ok = G.cur('coins') >= c;
              ups += `<button class="mini${ok ? ' ok' : ''}" data-a="buildings.up:${i}:${j}"${U.dis(ok)}>${U.mul(u.m)} · 🪙${U.fmt(c)}</button>`;
            } else ups += `<span class="mini lock">🔒${U.mul(u.m)} ур.${u.req}</span>`;
          });
          const share = lv ? ` · ${U.pct(M.one(i) / total, 0)} дохода` : '';
          h += UI.card({
            k: 'b' + i, icon: b.icon, title: b.name, lv: lv ? 'ур. ' + lv : '',
            desc: `+${U.fmt(per)}/с за ур.${share}`,
            extra: (nx ? UI.bar(lv / nx, 'thin', `×2 на ур. ${nx}`) : '<div class="tag">все вехи ×32</div>') + `<div class="minis">${ups}</div>`,
            btn: UI.buy('buildings.buy:' + i, o), cls: o.ok ? 'can' : 'cant',
          });
        }
        if (n < B.length) h += UI.card({ k: 'bnext', icon: '❔', title: '???', desc: `Купите «${B[n - 1].name}», чтобы увидеть следующее здание`, cls: 'cant' });
        return h;
      },
      badge() {
        const n = M.shown(), s = G.S.buildings;
        for (let i = 0; i < n; i++) {
          if (s.lv[i] === 0 && M.offerOf(i).ok) return true;
          for (let j = 0; j < 3; j++) if (!s.up[i][j] && s.lv[i] >= UPS[j].req && G.cur('coins') >= M.upCost(i, j)) return true;
        }
        return false;
      },
    }],

    actions: {
      buy(i) {
        const o = M.offerOf(i);
        if (!G.take(o)) { SFX.play('error'); return; }
        const before = G.S.buildings.lv[i];
        G.S.buildings.lv[i] += o.n;
        G.emit('buy', 'building', o.n, i);
        SFX.play('buy');
        const nx = nextMs(before);
        if (nx && G.S.buildings.lv[i] >= nx) G.toast(`${B[i].icon} ${B[i].name}: веха ${nx} — доход ×2!`, 'good');
      },
      up(i, j) {
        const s = G.S.buildings;
        if (s.up[i][j] || s.lv[i] < UPS[j].req) return;
        if (!G.pay({ coins: M.upCost(i, j) })) { SFX.play('error'); return; }
        s.up[i][j] = 1;
        G.emit('buy', 'bup', 1, i);
        SFX.play('buy');
        G.toast(`${B[i].icon} ${UPS[j].name}: ${B[i].name} ${U.mul(UPS[j].m)}`, 'good');
      },
    },
  });
  G.defStat('synergy', 1);   // сила синергии соседей
  G.defStat('bldMult', 1);   // общий множитель зданий
  G.defStat('masteryK', 1);  // сила мастерства
})();
