'use strict';
/* ============================================================
   boosters.js — бустеры: 6 видов временных усилений.
   ×2 доход, ×10 тап, авто-клик, золотой дождь, заморозка цен,
   удача. У каждого прокачиваются длительность, сила, перезарядка.
   Таймеры — в реальном времени (работают и после перезапуска).
   ============================================================ */
(() => {
  const BOOST = [
    { id: 'x2', name: 'Двойной доход', icon: '💰', dur: 30, cd: 300, need: 5e6, cost: 1e6,
      desc: (p) => `Весь доход ${U.mul(2 + 0.1 * p)}` },
    { id: 'tap', name: 'Ярость тапа', icon: '👊', dur: 20, cd: 300, need: 5e6, cost: 1.5e6,
      desc: (p) => `Доход с тапа ${U.mul(10 + p)}` },
    { id: 'auto', name: 'Авто-клик', icon: '🖱️', dur: 20, cd: 360, need: 5e7, cost: 1e7,
      desc: (p) => `${10 + 2 * p} тапов в секунду автоматически` },
    { id: 'rain', name: 'Золотой дождь', icon: '🌧️', dur: 15, cd: 480, need: 5e8, cost: 1e8,
      desc: (p) => `Падают монеты по ${(3 * (1 + 0.2 * p)).toFixed(1)} с дохода — лови!` },
    { id: 'freeze', name: 'Заморозка цен', icon: '🧊', dur: 20, cd: 600, need: 5e9, cost: 1e9,
      desc: (p) => `Следующие ${10 + 3 * p} уровней каждого товара — по текущей цене${p ? `, скидка ${3 * p}%` : ''}` },
    { id: 'luck', name: 'Удача', icon: '🍀', dur: 30, cd: 600, need: 5e10, cost: 1e10,
      desc: (p) => `+${20 + 3 * p}% к шансу крита, события ×3, добыча ×2` },
  ];
  const BY = {};
  BOOST.forEach((b) => (BY[b.id] = b));
  // прокачка: длительность, сила, перезарядка
  const UP = {
    d: { name: 'Длительность', r: 3, max: 20, k: 1 },
    p: { name: 'Сила', r: 3.2, max: 20, k: 1.5 },
    c: { name: 'Перезарядка', r: 3.5, max: 15, k: 2 },
  };
  let autoAcc = 0, rainAcc = 0;

  const M = G.mod({
    id: 'boosters',
    reset: 0,
    BOOST,
    state: () => {
      const u = {}, end = {}, ready = {};
      BOOST.forEach((b) => { u[b.id] = { d: 0, p: 0, c: 0 }; end[b.id] = 0; ready[b.id] = 0; });
      return { u, end, ready };
    },
    // прокачка бустеров сбрасывается на престиже, таймеры — нет
    onReset(layer) {
      if (layer >= 1) BOOST.forEach((b) => { G.S.boosters.u[b.id] = { d: 0, p: 0, c: 0 }; });
    },
    open: (b) => (G.S.life.coins || 0) >= b.need,
    dur(b) { return b.dur * (1 + 0.15 * G.S.boosters.u[b.id].d) * G.st.boostDur; },
    // перезарядка не короче 2.5 длительностей: бустер активен не больше ~30% времени
    cd(b) { return Math.max(this.dur(b) * 2.5, b.cd * (1 - 0.04 * G.S.boosters.u[b.id].c) * G.st.boostCd); },
    pw(b) { return G.S.boosters.u[b.id].p; },
    active(id) { return Date.now() < G.S.boosters.end[id]; },
    left(id) { return Math.max(0, (G.S.boosters.end[id] - Date.now()) / 1000); },
    cdLeft(id) { return Math.max(0, (G.S.boosters.ready[id] - Date.now()) / 1000); },
    upCost(b, k) { return b.cost * UP[k].k * Math.pow(UP[k].r, G.S.boosters.u[b.id][k]); },
    levels() { let n = 0; for (const b of BOOST) { const u = G.S.boosters.u[b.id]; n += u.d + u.p + u.c; } return n; },

    stats(acc) {
      const bp = G.st.boostPow || 1;
      if (this.active('x2')) acc.add('allTemp', (1 + 0.1 * this.pw(BY.x2)) * bp);
      if (this.active('tap')) acc.add('tapTemp', (9 + this.pw(BY.tap)) * bp);
      if (this.active('freeze')) acc.mul('cost', 1 - Math.min(0.6, 0.03 * this.pw(BY.freeze)));
      if (this.active('luck')) {
        acc.add('critCh', (0.2 + 0.03 * this.pw(BY.luck)) * bp);
        acc.mul('evFreq', 3);
        acc.mul('drop', 2);
      }
    },

    tick(dt) {
      // заморозка цен: снимок уровней при включении
      const fr = this.active('freeze');
      if (fr && !G.freezeMap) G.freezeMap = {};
      G.freezeCap = 10 + 3 * this.pw(BY.freeze);
      if (!fr && G.freezeMap) { G.freezeMap = null; G.dirty = true; }
      // авто-клик: работает как ручные тапы (растит комбо)
      if (this.active('auto')) {
        autoAcc += (10 + 2 * this.pw(BY.auto)) * (G.st.boostPow || 1) * dt;
        while (autoAcc >= 1) {
          autoAcc--;
          const r = document.getElementById('orb').getBoundingClientRect();
          const x = r.left + r.width * (0.25 + Math.random() * 0.5), y = r.top + r.height * (0.25 + Math.random() * 0.5);
          const ctx = G.doTap(x, y, false);
          if (UI.tab === 'tap' && !document.hidden) G.byId.tap.tapFx(ctx, document.getElementById('orb'));
        }
      }
      // золотой дождь
      if (this.active('rain')) {
        rainAcc += dt * 3;
        while (rainAcc >= 1) { rainAcc--; spawnCoin(); }
      }
    },

    use(id) {
      const b = BY[id], s = G.S.boosters;
      if (!b || !this.open(b) || this.cdLeft(id) > 0) { SFX.play('error'); return; }
      const now = Date.now();
      s.end[id] = now + this.dur(b) * 1000;
      s.ready[id] = now + this.cd(b) * 1000;
      G.count('boosts');
      G.emit('booster', id);
      SFX.play('boost');
      FX.flash('rgba(34,230,255,.25)');
      G.toast(`${b.icon} ${b.name}: ${b.desc(this.pw(b))} на ${Math.round(this.dur(b))} с`, 'good');
    },
    rainValue() { return Math.max(5, G.income() * 3 * (1 + 0.2 * this.pw(BY.rain)) * (G.st.boostPow || 1)); },

    panels: [{
      id: 'boosters', group: 'upg', name: 'Бустеры', icon: '🚀', order: 5,
      unlock: () => (G.S.life.coins || 0) >= 5e6,
      hint: '5M монет за всё время',
      render() {
        let h = UI.info('Бустеры включаются кнопками на экране тапа. Прокачка сбрасывается при престиже.');
        for (const b of BOOST) {
          if (!M.open(b)) { h += UI.card({ k: 'bo' + b.id, icon: '🔒', title: b.name, desc: `Откроется при ${U.fmt(b.need)} монет за всё время`, cls: 'cant' }); continue; }
          const u = G.S.boosters.u[b.id];
          let minis = '';
          for (const k in UP) {
            if (u[k] >= UP[k].max) { minis += `<span class="mini done">${UP[k].name} МАКС</span>`; continue; }
            const c = M.upCost(b, k) * G.st.cost, ok = G.cur('coins') >= c;
            minis += `<button class="mini${ok ? ' ok' : ''}" data-a="boosters.up:${b.id}:${k}"${U.dis(ok)}>${UP[k].name} ${u[k]} · 🪙${U.fmt(c)}</button>`;
          }
          const st = M.active(b.id) ? `<span class="good">активен ${Math.ceil(M.left(b.id))} с</span>` : M.cdLeft(b.id) > 0 ? `перезарядка ${U.time(M.cdLeft(b.id))}` : '<span class="good">готов</span>';
          h += UI.card({
            k: 'bo' + b.id, icon: b.icon, title: b.name,
            desc: `${b.desc(u.p)}<br>⏱ ${Math.round(M.dur(b))} с · ♻️ ${U.time(M.cd(b))} · ${st}`,
            extra: `<div class="minis">${minis}</div>`,
          });
        }
        return h;
      },
    }],
    actions: {
      use(id) { M.use(id); },
      up(id, k) {
        const b = BY[id], u = G.S.boosters.u[id];
        if (u[k] >= UP[k].max) return;
        if (!G.pay({ coins: M.upCost(b, k) * G.st.cost })) { SFX.play('error'); return; }
        u[k]++;
        G.emit('buy', 'booster', 1);
        SFX.play('buy');
      },
    },
  });
  G.defStat('boostDur', 1);
  G.defStat('boostPow', 1);
  G.defStat('boostCd', 1);
  G.defStat('evFreq', 1);
  G.defStat('drop', 1);

  // монетка золотого дождя
  function spawnCoin() {
    if (UI.tab !== 'tap' || document.hidden) { G.earn(M.rainValue() * 0.5, 'rain'); return; }
    const layer = document.getElementById('evLayer');
    if (layer.querySelectorAll('.rcoin').length > 14) return;
    const c = document.createElement('button');
    c.className = 'rcoin';
    c.textContent = U.pick(['🪙', '🪙', '💰', '🪙']);
    c.style.left = U.rand(4, 86) + '%';
    c.style.animationDuration = U.rand(2.4, 3.6) + 's';
    c.addEventListener('pointerdown', (e) => {
      e.preventDefault(); e.stopPropagation();
      const v = M.rainValue();
      G.earn(v, 'rain');
      G.count('rainCoins');
      FX.float(e.clientX, e.clientY, '+' + U.fmt(v), '#ffd34d', 22);
      FX.burst(e.clientX, e.clientY, 10, ['#ffd34d', '#fff3b0']);
      SFX.play('coin');
      c.remove();
    });
    c.addEventListener('animationend', () => c.remove());
    layer.appendChild(c);
  }

  // кнопки бустеров на экране тапа
  UI.view('tapBottom', 10, () => {
    if (!G.has('boosters')) return '';
    let h = '<div class="boosts">';
    for (const b of BOOST) {
      if (!M.open(b)) continue;
      const act = M.active(b.id), cdl = M.cdLeft(b.id);
      const p = act ? M.left(b.id) / M.dur(b) : cdl > 0 ? cdl / M.cd(b) : 0;
      const t = act ? Math.ceil(M.left(b.id)) + 'с' : cdl > 0 ? (cdl >= 60 ? Math.ceil(cdl / 60) + 'м' : Math.ceil(cdl) + 'с') : '';
      h += `<button class="bst${act ? ' act' : cdl > 0 ? ' cd' : ' rdy'}" data-a="boosters.use:${b.id}" data-k="${b.id}" style="--p:${p.toFixed(3)}" aria-label="${b.name}">
        <span class="bi">${b.icon}</span>${t ? `<span class="bt">${t}</span>` : ''}</button>`;
    }
    return h + '</div>';
  });
})();
