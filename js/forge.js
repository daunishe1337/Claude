'use strict';
/* ============================================================
   forge.js — кузница.
   Крафт экипировки из ресурсов шахты. Уровень кузницы открывает
   рецепты более высокой редкости и повышает силу предметов.
   ============================================================ */
(() => {
  // рецепты: минимальная редкость, требуемый уровень кузницы, стоимость
  const RECIPES = [
    { r: 1, lv: 1, cost: { wood: 60, stone: 40 } },
    { r: 2, lv: 2, cost: { stone: 150, metal: 40 } },
    { r: 3, lv: 4, cost: { metal: 200, gems: 25 } },
    { r: 4, lv: 7, cost: { metal: 600, gems: 120, ether: 5 } },
    { r: 5, lv: 10, cost: { gems: 500, ether: 40, souls: 200 } },
  ];
  const lvCost = (l) => ({ stone: Math.round(80 * Math.pow(1.7, l)), metal: Math.round(20 * Math.pow(1.75, l)), gems: l >= 3 ? Math.round(5 * Math.pow(1.8, l - 3)) : 0 });
  let slot = 'gloves';

  const M = G.mod({
    id: 'forge',
    reset: 3,
    state: () => ({ lv: 1, crafted: 0 }),
    levels() { return G.S.forge.lv; },
    panels: [{
      id: 'forge', group: 'hero', name: 'Кузница', icon: '⚒️', order: 8,
      unlock: () => G.has('mine') && G.has('gear'),
      hint: 'Откройте шахту и экипировку',
      render() {
        const s = G.S.forge, gear = G.byId.gear;
        let h = `<div class="pcard"><div class="row"><b>⚒️ Кузница · уровень ${s.lv}</b><span class="dim">создано ${s.crafted}</span></div>
          <div class="center dim small">Сила предметов кузницы: уровень ${s.lv * 2 + G.byId.stage.worlds()}</div>`;
        const lc = lvCost(s.lv);
        for (const k in lc) if (!lc[k]) delete lc[k];
        h += `<button class="btn gold wide" data-a="forge.up"${U.dis(G.can(lc))}>⬆️ Улучшить кузницу · ${UI.costs(lc)}</button></div>`;
        h += '<div class="resrow">' + ['wood', 'stone', 'metal', 'gems', 'ether'].map((id) => `<span>${G.CUR[id].icon} ${U.fint(G.cur(id))}</span>`).join('') + '</div>';
        h += UI.sec('Слот предмета');
        h += '<div class="chips small">' + Object.keys(gear.SLOTS).map((sl) => `<button class="chip${slot === sl ? ' on' : ''}" data-a="forge.slot:${sl}">${gear.SLOTS[sl][1]} ${gear.SLOTS[sl][0]}</button>`).join('') + '</div>';
        h += UI.sec('Рецепты');
        RECIPES.forEach((rc, i) => {
          const R = gear.RAR[rc.r], open = s.lv >= rc.lv;
          h += UI.card({
            k: 'rc' + i, icon: `<span style="filter:drop-shadow(0 0 6px ${R.c})">${gear.SLOTS[slot][1]}</span>`,
            title: `<span style="color:${R.c}">${R.name}+</span>`, desc: open ? UI.costs(rc.cost) : `🔒 Кузница ур. ${rc.lv}`,
            btn: open ? UI.buyC('forge.craft:' + i, rc.cost, 'Создать') : '', cls: open ? (G.can(rc.cost) ? 'can' : 'cant') : 'cant',
          });
        });
        return h;
      },
    }],
    actions: {
      slot(sl) { slot = sl; },
      up() {
        const c = lvCost(G.S.forge.lv);
        for (const k in c) if (!c[k]) delete c[k];
        if (!G.pay(c)) { SFX.play('error'); return; }
        G.S.forge.lv++;
        SFX.play('level');
        G.toast(`⚒️ Кузница: уровень ${G.S.forge.lv}`, 'good');
      },
      craft(i) {
        const rc = RECIPES[i], s = G.S.forge, gear = G.byId.gear;
        if (!rc || s.lv < rc.lv || !G.pay(rc.cost)) { SFX.play('error'); return; }
        const it = gear.make(s.lv * 2 + G.byId.stage.worlds(), rc.r, slot);
        gear.give(it);
        s.crafted++;
        G.count('crafted');
        SFX.play('win');
      },
    },
  });
  void M;
})();
