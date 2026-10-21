'use strict';
/* ============================================================
   alchemy.js — алхимия.
   Зелья варятся из урожая фермы и хранятся в сумке. Выпитое
   зелье даёт временный эффект (повторное — продлевает).
   Сила зелий растёт от класса Алхимик и других бонусов.
   ============================================================ */
(() => {
  // [id, название, иконка, рецепт, минут, эффект(acc, k), описание(k)]
  const POT = [
    ['power', 'Зелье силы', '💪', { wheat: 10, carrot: 5 }, 10, (a, k) => a.add('tapTemp', 2 * k), (k) => `Тап ×${U.fmt(1 + 2 * k)}`],
    ['wealth', 'Зелье богатства', '💰', { wheat: 15, berry: 5 }, 10, (a, k) => a.add('allTemp', 1 * k), (k) => `Весь доход ×${U.fmt(1 + k)}`],
    ['luck', 'Зелье удачи', '🍀', { berry: 10, carrot: 10 }, 15, (a, k) => { a.add('critCh', 0.1 * k); a.mul('evFreq', 1 + k); }, (k) => `+${U.pct(0.1 * k, 0)} крита, события ×${U.fmt(1 + k)}`],
    ['soul', 'Зелье душ', '👻', { pumpkin: 5, berry: 10 }, 30, (a, k) => a.mul('souls', 1 + k), (k) => `Души ×${U.fmt(1 + k)}`],
    ['wisdom', 'Эликсир мудрости', '📘', { mushroom: 3, wheat: 20 }, 30, (a, k) => a.mul('xp', 1 + 2 * k), (k) => `Опыт ×${U.fmt(1 + 2 * k)}`],
    ['mana', 'Эликсир маны', '🔮', { carrot: 10, berry: 5 }, 10, (a, k) => a.mul('manaRegen', 1 + 2 * k), (k) => `Полная мана, реген ×${U.fmt(1 + 2 * k)}`],
    ['time', 'Эликсир времени', '⏳', { mushroom: 5, pumpkin: 5 }, 0, null, (k) => `Мгновенно ${U.time(3600 * k)} дохода зданий`],
    ['star', 'Звёздный эликсир', '✨', { star: 2, mushroom: 5 }, 60, (a, k) => a.mul('crystal', 1 + 0.25 * k), (k) => `Кристаллы ×${U.fmt(1 + 0.25 * k)}`],
  ].map(([id, name, icon, rec, min, st, desc]) => ({ id, name, icon, rec, min, st, desc }));
  const BY = {};
  POT.forEach((p) => (BY[p.id] = p));

  const M = G.mod({
    id: 'alchemy',
    reset: 3,
    state: () => ({ inv: {}, act: {}, brewed: 0 }),
    k: () => G.st.potion,
    stats(acc) {
      const now = Date.now(), s = G.S.alchemy;
      for (const id in s.act) if (s.act[id] > now && BY[id].st) BY[id].st(acc, this.k());
    },
    levels() { return Math.floor(G.S.alchemy.brewed / 5); },
    panels: [{
      id: 'alchemy', group: 'more', name: 'Алхимия', icon: '⚗️', order: 42,
      unlock: () => (G.S.stats.harvests || 0) >= 3,
      hint: '3 урожая на ферме',
      render() {
        const s = G.S.alchemy, now = Date.now();
        let h = UI.info(`Сила зелий: <b>${U.mul(M.k())}</b>. Повторное зелье продлевает эффект.`);
        const act = POT.filter((p) => (s.act[p.id] || 0) > now);
        if (act.length) h += '<div class="minis">' + act.map((p) => `<span class="mini done">${p.icon} ${U.time((s.act[p.id] - now) / 1000)}</span>`).join('') + '</div>';
        h += '<div class="resrow">' + G.byId.farm.CROPS.map((c) => `<span>${c.icon} ${U.fint(G.cur(c.id))}</span>`).join('') + '</div>';
        for (const p of POT) {
          const n = s.inv[p.id] || 0;
          h += UI.card({
            k: 'po' + p.id, icon: p.icon, title: p.name, lv: n ? '×' + n : '',
            desc: `${p.desc(M.k())}${p.min ? ' · ' + p.min + ' мин' : ''}<br>${UI.costs(p.rec)}`,
            btn: UI.buyC('alchemy.brew:' + p.id, p.rec, 'Сварить') + (n ? `<button class="buy ok" data-a="alchemy.drink:${p.id}">Выпить</button>` : ''),
            cls: n ? 'can' : G.can(p.rec) ? '' : 'cant',
          });
        }
        return h;
      },
      badge: () => Object.values(G.S.alchemy.inv).some((n) => n > 0) && !Object.values(G.S.alchemy.act).some((t) => t > Date.now()),
    }],
    actions: {
      brew(id) {
        const p = BY[id];
        if (!p || !G.pay(p.rec)) { SFX.play('error'); return; }
        G.S.alchemy.inv[id] = (G.S.alchemy.inv[id] || 0) + 1;
        G.S.alchemy.brewed++;
        G.count('potions');
        SFX.play('open');
      },
      drink(id) {
        const s = G.S.alchemy, p = BY[id];
        if (!p || !(s.inv[id] > 0)) return;
        s.inv[id]--;
        if (id === 'time') { const v = G.cps * 3600 * M.k(); G.earn(v, 'skill'); G.toast(`⏳ +${U.fmt(v)} 🪙`, 'good'); }
        else {
          if (id === 'mana') G.S.skills.mana = G.byId.skills.maxMana();
          s.act[id] = Math.max(s.act[id] || 0, Date.now()) + p.min * 60000;
          G.toast(`${p.icon} ${p.name}: ${p.desc(M.k())}`, 'good');
        }
        G.count('drunk');
        SFX.play('boost');
        FX.flash('rgba(163,104,255,.3)');
        G.dirty = true;
      },
    },
  });
  void M;
  G.achTier('farm', 'po', ['Зельевар', 'Алхимик'], () => G.S.stats.potions || 0, [1, 50], (t) => `Сварить ${t} зелий`);
})();
