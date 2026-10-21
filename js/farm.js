'use strict';
/* ============================================================
   farm.js — ферма.
   Грядки, рост по реальному времени (работает офлайн), урожай
   даёт культуры для алхимии и временный баф. Прокачка числа
   грядок, скорости роста и урожайности. В дождь растёт быстрее.
   ============================================================ */
(() => {
  // [id, название, иконка, секунд роста, урожай, баф: стат, значение, минут, нужно сборов для открытия]
  const CROPS = [
    ['wheat', 'Пшеница', '🌾', 60, 3, 'cps', 0.1, 5, 0],
    ['carrot', 'Морковь', '🥕', 300, 3, 'tap', 0.2, 10, 5],
    ['berry', 'Ягоды', '🍓', 1200, 3, 'evReward', 0.3, 15, 20],
    ['pumpkin', 'Тыква', '🎃', 3600, 2, 'souls', 0.2, 30, 50],
    ['mushroom', 'Волшебный гриб', '🍄', 10800, 2, 'all', 0.15, 30, 100],
    ['star', 'Звёздоцвет', '🌸', 28800, 1, 'crystal', 0.1, 60, 200],
  ].map(([id, name, icon, sec, yld, stat, v, min, need]) => ({ id, name, icon, sec, yld, stat, v, min, need }));
  const BY = {};
  CROPS.forEach((c) => { BY[c.id] = c; G.defCur(c.id, c.name, c.icon, 3); });
  const UPG = {
    plots: { name: 'Новая грядка', icon: '🟫', max: 9, cost: (l) => ({ wood: Math.round(40 * Math.pow(2, l)), stone: Math.round(20 * Math.pow(2, l)) }), val: (l) => (3 + l) + ' грядок' },
    speed: { name: 'Удобрения', icon: '🌱', max: 30, cost: (l) => ({ wheat: Math.round(10 * Math.pow(1.5, l)), wood: Math.round(30 * Math.pow(1.45, l)) }), val: (l) => 'рост ' + U.mul(1 + 0.1 * l) },
    yld: { name: 'Урожайность', icon: '🧺', max: 30, cost: (l) => ({ carrot: Math.round(8 * Math.pow(1.5, l)), stone: Math.round(30 * Math.pow(1.45, l)) }), val: (l) => 'урожай ' + U.mul(1 + 0.15 * l) },
  };

  const M = G.mod({
    id: 'farm',
    reset: 3,
    CROPS,
    state: () => ({ plots: [null, null, null], plotsLv: 0, speed: 0, yld: 0, seed: 'wheat', buffs: {}, harvests: 0 }),
    speedMul() { return (1 + 0.1 * G.S.farm.speed) * G.st.farmSpeed; },
    growT(c) { return c.sec / this.speedMul(); },
    open: (c) => G.S.farm.harvests >= c.need,
    // актуализировать число грядок
    init() { const s = G.S.farm; while (s.plots.length < 3 + s.plotsLv) s.plots.push(null); },
    stats(acc) {
      const now = Date.now();
      for (const id in G.S.farm.buffs) if (G.S.farm.buffs[id] > now) G.applyStat(acc, BY[id].stat, BY[id].v * G.st.farm);
    },
    levels() { const s = G.S.farm; return s.plotsLv + s.speed + s.yld + Math.floor(s.harvests / 10); },
    // грядка: { crop, prog — накопленные секунды роста }; скорость учитывается каждый тик
    ready(p) { return p && p.prog >= BY[p.crop].sec; },
    left(p) { return Math.max(0, (BY[p.crop].sec - p.prog) / this.speedMul()); },
    plant(i, id) {
      const s = G.S.farm, c = BY[id];
      if (s.plots[i] || !c || !this.open(c)) return false;
      s.plots[i] = { crop: id, prog: 0 };
      return true;
    },
    harvest(i) {
      const s = G.S.farm, p = s.plots[i];
      if (!this.ready(p)) return null;
      const c = BY[p.crop];
      const n = Math.max(1, Math.round(c.yld * (1 + 0.15 * s.yld) * G.st.farm));
      G.add(c.id, n);
      s.buffs[c.id] = Math.max(s.buffs[c.id] || 0, Date.now()) + c.min * 60000;
      s.plots[i] = null;
      s.harvests++;
      G.count('harvests');
      G.emit('harvest', c.id);
      G.dirty = true;
      return { c, n };
    },
    grow(sec) {
      const k = sec * this.speedMul();
      for (const p of G.S.farm.plots) if (p && !this.ready(p)) p.prog += k;
    },
    tick(dt) { this.grow(dt); },
    offline(sec) { this.grow(sec); },
    panels: [{
      id: 'farm', group: 'more', name: 'Ферма', icon: '🌾', order: 41,
      unlock: () => G.has('mine'),
      hint: 'Откройте шахту',
      render() {
        const s = G.S.farm, now = Date.now();
        let h = UI.info(`Растения растут в реальном времени, даже когда игра закрыта. Рост ${U.mul(M.speedMul())}${G.st.farmSpeed > 1.01 ? ' (с учётом погоды и бонусов)' : ''}. Урожай даёт культуры для алхимии и временный баф.`);
        // активные бафы
        const act = CROPS.filter((c) => (s.buffs[c.id] || 0) > now);
        if (act.length) h += '<div class="minis">' + act.map((c) => `<span class="mini done">${c.icon} ${G.statText(c.stat, c.v * G.st.farm)} · ${U.time((s.buffs[c.id] - now) / 1000)}</span>`).join('') + '</div>';
        h += UI.sec('Семена', 'выберите, что сажать');
        h += '<div class="chips small">' + CROPS.map((c) => M.open(c)
          ? `<button class="chip${s.seed === c.id ? ' on' : ''}" data-a="farm.seed:${c.id}">${c.icon} ${U.time(M.growT(c))}</button>`
          : `<span class="chip" style="opacity:.4">🔒 ${c.need} сборов</span>`).join('') + '</div>';
        h += '<div class="btnrow"><button class="btn pri" data-a="farm.plantAll">🌱 Засеять всё</button><button class="btn gold" data-a="farm.harvestAll">🧺 Собрать всё</button></div>';
        h += '<div class="plots">';
        s.plots.forEach((p, i) => {
          if (!p) { h += `<button class="plot empty" data-a="farm.plant:${i}" data-k="pl${i}"><span>➕</span><small>посадить</small></button>`; return; }
          const c = BY[p.crop], ready = M.ready(p), k = U.clamp(p.prog / c.sec, 0, 1);
          h += `<button class="plot${ready ? ' ready' : ''}" data-a="farm.harvest:${i}" data-k="pl${i}"><span style="transform:scale(${(0.5 + 0.5 * k).toFixed(2)})">${ready ? c.icon : k < 0.5 ? '🌱' : '🌿'}</span>
            <small>${ready ? 'собрать!' : U.time(M.left(p))}</small><i style="width:${(k * 100).toFixed(0)}%"></i></button>`;
        });
        h += '</div>';
        h += '<div class="resrow">' + CROPS.map((c) => `<span>${c.icon} ${U.fint(G.cur(c.id))}</span>`).join('') + '</div>';
        h += UI.sec('Улучшения фермы');
        for (const k in UPG) {
          const d = UPG[k], l = k === 'plots' ? s.plotsLv : s[k], max = l >= d.max, c = d.cost(l);
          h += UI.card({ k: 'fu' + k, icon: d.icon, title: d.name, lv: `ур. ${l}`, desc: `${d.val(l)}${max ? '' : ` → <span class="hl">${d.val(l + 1)}</span>`}`,
            btn: max ? '<button class="buy max" disabled>МАКС</button>' : UI.buyC('farm.up:' + k, c), cls: max ? 'done' : G.can(c) ? 'can' : 'cant' });
        }
        return h;
      },
      badge: () => G.S.farm.plots.some((p) => !p || M.ready(p)),
    }],
    actions: {
      seed(id) { G.S.farm.seed = id; },
      plant(i) { if (M.plant(i, G.S.farm.seed)) SFX.play('buy'); },
      harvest(i) {
        const r = M.harvest(i);
        if (r) { SFX.play('coin'); G.toast(`${r.c.icon} +${r.n} ${r.c.name} · баф: ${G.statText(r.c.stat, r.c.v * G.st.farm)}`, 'good'); }
      },
      plantAll() { let n = 0; G.S.farm.plots.forEach((p, i) => { if (!p && M.plant(i, G.S.farm.seed)) n++; }); if (n) SFX.play('buy'); },
      harvestAll() {
        const got = {};
        G.S.farm.plots.forEach((p, i) => { const r = M.harvest(i); if (r) got[r.c.icon] = (got[r.c.icon] || 0) + r.n; });
        const t = Object.keys(got).map((k) => k + ' ' + got[k]).join(' · ');
        if (t) { SFX.play('coin'); G.toast('🧺 Урожай: ' + t, 'good'); }
      },
      up(k) {
        const s = G.S.farm, d = UPG[k], key = k === 'plots' ? 'plotsLv' : k;
        if (s[key] >= d.max || !G.pay(d.cost(s[key]))) { SFX.play('error'); return; }
        s[key]++;
        if (k === 'plots') s.plots.push(null);
        SFX.play('buy');
      },
    },
  });
  G.defStat('farm', 1);
  G.defStat('farmSpeed', 1);
  G.achCat('farm', 'Ферма', '🌾', 'Урожай ×1.5', (a) => a.mul('farm', 1.5));
  G.achTier('farm', 'fh', ['Огородник', 'Фермер', 'Агроном', 'Повелитель полей'], () => G.S.stats.harvests || 0, [1, 50, 250, 1000], (t) => `Собрать урожай ${t} раз`);
})();
