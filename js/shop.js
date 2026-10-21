'use strict';
/* ============================================================
   shop.js — магазин редкой валюты (души).
   Ассортимент из 6 товаров меняется каждые 4 часа (одинаково
   на любом устройстве — по времени). Каждый товар можно купить
   один раз за ротацию; ассортимент можно обновить за души.
   ============================================================ */
(() => {
  const ROT = 4 * 3600000;
  // товар: [id, название, иконка, цена в душах, выдача, вес]
  const POOL = [
    ['egg', 'Яйцо', '🥚', 15, () => G.add('eggs', 1), 10],
    ['egg5', '5 яиц', '🥚', 65, () => G.add('eggs', 5), 6],
    ['scroll3', '3 свитка', '📜', 30, () => G.add('scrolls', 3), 8],
    ['food', '300 корма', '🍖', 20, () => G.add('food', 300), 8],
    ['rune', 'Руна 2 ур.', '🔮', 35, () => G.byId.gear.giveRune(U.pick(Object.keys(G.byId.gear.RUNES)), 2), 6],
    ['rune3', 'Руна 3 ур.', '🔮', 90, () => G.byId.gear.giveRune(U.pick(Object.keys(G.byId.gear.RUNES)), 3), 3],
    ['pack', 'Пак карт', '🃏', 22, () => { G.S.cards.packs++; }, 8],
    ['item', 'Эпический предмет', '🎁', 80, () => G.byId.gear.give(G.byId.gear.make(2 + G.byId.stage.worlds(), 3)), 4],
    ['item5', 'Легендарный предмет', '🎁', 220, () => G.byId.gear.give(G.byId.gear.make(3 + G.byId.stage.worlds(), 4)), 2],
    ['key', 'Ключ подземелья', '🗝️', 25, () => { G.S.dungeon.keys++; }, 6],
    ['stars', '3 звезды', '⭐', 60, () => G.add('stars', 3), 5],
    ['boost', 'Сброс бустеров', '🚀', 10, () => { for (const k in G.S.boosters.ready) G.S.boosters.ready[k] = 0; }, 6],
    ['scrap', '500 обломков', '⚙️', 30, () => G.add('scrap', 500), 6],
    ['ether', '20 эфира', '🌀', 70, () => G.add('ether', 20), 3],
  ].map(([id, name, icon, cost, give, w]) => ({ id, name, icon, cost, give, w }));

  const M = G.mod({
    id: 'shop',
    reset: 9,
    state: () => ({ slot: -1, reroll: 0, bought: [] }),
    slot: () => Math.floor(Date.now() / ROT),
    check() { const s = G.S.shop; if (s.slot !== this.slot()) { s.slot = this.slot(); s.reroll = 0; s.bought = []; } },
    offers() {
      this.check();
      const rnd = U.seeded(this.slot() * 1009 + G.S.shop.reroll * 7 + 13);
      const pool = POOL.slice(), out = [];
      while (out.length < 6 && pool.length) {
        const x = U.wpick(pool, (p) => p.w, rnd);
        out.push(x);
        pool.splice(pool.indexOf(x), 1);
      }
      return out;
    },
    rerollCost() { return 20 * (1 + G.S.shop.reroll); },
    panels: [{
      id: 'shop', group: 'more', name: 'Магазин', icon: '🛒', order: 16,
      unlock: () => (G.S.life.souls || 0) >= 40,
      hint: '40 душ за всё время',
      render() {
        const s = G.S.shop, list = M.offers();
        const left = (ROT - (Date.now() % ROT)) / 1000;
        let h = `<div class="row"><span>👻 Души: <b>${U.fint(G.cur('souls'))}</b></span><span class="dim">обновление через ${U.time(left)}</span></div>`;
        h += '<div class="shopgrid">';
        for (const o of list) {
          const sold = s.bought.includes(o.id), ok = !sold && G.cur('souls') >= o.cost;
          h += `<div class="offer${sold ? ' sold' : ''}" data-k="o${o.id}"><span>${o.icon}</span><b>${o.name}</b>
            ${sold ? '<small>куплено</small>' : `<button class="mini${ok ? ' ok' : ''}" data-a="shop.buy:${o.id}"${U.dis(ok)}>👻 ${o.cost}</button>`}</div>`;
        }
        h += '</div>';
        h += `<button class="btn wide" data-a="shop.reroll"${U.dis(G.cur('souls') >= M.rerollCost())}>🔄 Обновить ассортимент (👻 ${M.rerollCost()})</button>`;
        return h;
      },
    }],
    actions: {
      buy(id) {
        const s = G.S.shop, o = M.offers().find((x) => x.id === id);
        if (!o || s.bought.includes(id) || !G.pay({ souls: o.cost })) { SFX.play('error'); return; }
        s.bought.push(id);
        o.give();
        G.count('shopBuys');
        SFX.play('buy');
        G.toast(`🛒 ${o.icon} ${o.name}`, 'good');
      },
      reroll() {
        if (!G.pay({ souls: M.rerollCost() })) { SFX.play('error'); return; }
        G.S.shop.reroll++; G.S.shop.bought = [];
        SFX.play('open');
      },
    },
  });
  void M;
})();
