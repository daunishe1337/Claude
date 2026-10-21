'use strict';
/* ============================================================
   city.js — город.
   Ратуша, казармы, библиотека, рынок, храм и обсерватория.
   Строятся из ресурсов шахты, каждое здание даёт глобальный
   бонус, растущий с уровнем.
   ============================================================ */
(() => {
  // [id, название, иконка, описание(L), эффект(acc, L), множитель цены]
  const BLD = [
    ['hall', 'Ратуша', '🏛️', (l) => `Весь доход +${10 * l}%`, (a, l) => a.mul('all', 1 + 0.1 * l), 1],
    ['barracks', 'Казармы', '🏰', (l) => `Армия: +${5 * l} к лимиту, сила +${10 * l}%`, (a, l) => { a.add('armyCap', 5 * l); a.mul('army', 1 + 0.1 * l); }, 1.2],
    ['library', 'Библиотека', '📚', (l) => `Опыт +${10 * l}%, исследования +${5 * l}%`, (a, l) => { a.mul('xp', 1 + 0.1 * l); a.mul('research', 1 + 0.05 * l); }, 1.4],
    ['market', 'Рынок', '🏪', (l) => `Цены −${Math.min(30, l)}%, спред биржи −${(0.5 * l).toFixed(1)}%`, (a, l) => a.mul('cost', 1 - Math.min(0.3, 0.01 * l)), 1.3],
    ['temple', 'Храм', '⛩️', (l) => `Души +${10 * l}%, кристаллы +${3 * l}%`, (a, l) => { a.mul('souls', 1 + 0.1 * l); a.mul('crystal', 1 + 0.03 * l); }, 1.8],
    ['observ', 'Обсерватория', '🔭', (l) => `Звёздная пыль +${5 * l}%, события +${5 * l}%`, (a, l) => { a.mul('dust', 1 + 0.05 * l); a.mul('evFreq', 1 + 0.05 * l); }, 2.2],
  ].map(([id, name, icon, desc, st, k]) => ({ id, name, icon, desc, st, k }));
  const MAXL = 50;
  const cost = (b, l) => {
    const c = { wood: Math.round(60 * b.k * Math.pow(1.45, l)), stone: Math.round(40 * b.k * Math.pow(1.47, l)) };
    if (l >= 3) c.metal = Math.round(15 * b.k * Math.pow(1.5, l - 3));
    if (l >= 10) c.gems = Math.round(5 * b.k * Math.pow(1.5, l - 10));
    if (l >= 25) c.ether = Math.round(2 * b.k * Math.pow(1.45, l - 25));
    return c;
  };

  const M = G.mod({
    id: 'city',
    reset: 3,
    BLD,
    state: () => { const lv = {}; BLD.forEach((b) => (lv[b.id] = 0)); return { lv }; },
    stats(acc) { for (const b of BLD) { const l = G.S.city.lv[b.id]; if (l) b.st(acc, l); } },
    levels() { let n = 0; for (const b of BLD) n += G.S.city.lv[b.id]; return n; },
    panels: [{
      id: 'city', group: 'more', name: 'Город', icon: '🏙️', order: 45,
      unlock: () => G.has('mine') && G.S.mine.maxFloor >= 3,
      hint: 'Этаж шахты 3',
      render() {
        let h = UI.info('Здания города строятся из ресурсов шахты и дают глобальные бонусы.');
        h += '<div class="resrow">' + ['wood', 'stone', 'metal', 'gems', 'ether'].map((id) => `<span>${G.CUR[id].icon} ${U.fint(G.cur(id))}</span>`).join('') + '</div>';
        h += '<div class="cityview">' + BLD.map((b) => `<span class="cb" style="font-size:${22 + Math.min(18, G.S.city.lv[b.id])}px">${b.icon}</span>`).join('') + '</div>';
        for (const b of BLD) {
          const l = G.S.city.lv[b.id], max = l >= MAXL, c = cost(b, l);
          h += UI.card({ k: 'c' + b.id, icon: b.icon, title: b.name, lv: `ур. ${l}`, desc: `${l ? b.desc(l) : 'Не построено'}${max ? '' : ` → <span class="hl">${b.desc(l + 1)}</span>`}`,
            btn: max ? '<button class="buy max" disabled>МАКС</button>' : UI.buyC('city.up:' + b.id, c, l ? 'Улучшить' : 'Построить'), cls: max ? 'done' : G.can(c) ? 'can' : 'cant' });
        }
        return h;
      },
      badge: () => BLD.some((b) => G.S.city.lv[b.id] === 0 && G.can(cost(b, 0))),
    }],
    actions: {
      up(id) {
        const b = BLD.find((x) => x.id === id), s = G.S.city;
        if (!b || s.lv[id] >= MAXL || !G.pay(cost(b, s.lv[id]))) { SFX.play('error'); return; }
        s.lv[id]++;
        SFX.play('level');
        G.emit('buy', 'city', 1);
      },
    },
  });
  void M;
  G.defStat('army', 1);
  G.defStat('armyCap', 0);
  G.achCat('city', 'Город', '🏙️', 'Весь доход +25%', (a) => a.mul('all', 1.25));
  G.achTier('city', 'ct', ['Поселение', 'Городок', 'Город', 'Мегаполис'], () => M.levels(), [5, 25, 75, 200], (t) => `Суммарный уровень города ${t}`);
})();
