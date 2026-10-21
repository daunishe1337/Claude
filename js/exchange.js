'use strict';
/* ============================================================
   exchange.js — биржа.
   Курсы ресурсов (в монетах) меняются каждые 30 секунд:
   плавная волна + случайный шум. Цена привязана к текущему
   доходу, поэтому биржа полезна на любом этапе игры.
   Покупка дороже продажи (спред), рынок в городе снижает спред.
   Крупные сделки сдвигают цену (насыщение рынка), со временем
   рынок восстанавливается.
   ============================================================ */
(() => {
  // ресурс: базовая цена в секундах дохода
  const GOODS = [['wood', 0.5], ['stone', 0.8], ['metal', 3], ['gems', 20], ['ether', 120], ['wheat', 0.3], ['carrot', 1], ['berry', 3], ['pumpkin', 10], ['mushroom', 30], ['star', 100]];
  const SLOT = 30000; // смена курса каждые 30 с
  let sel = 'wood';

  // множитель курса в момент slot (детерминированный)
  function mult(id, slot) {
    let h = 0;
    for (let i = 0; i < id.length; i++) h = (h * 31 + id.charCodeAt(i)) | 0;
    const r = U.seeded(slot * 7919 + h)();
    return Math.exp(0.45 * Math.sin(slot * 0.21 + h % 7) + 0.25 * Math.sin(slot * 0.057 + h % 13) + 0.3 * (r - 0.5));
  }

  const M = G.mod({
    id: 'exchange',
    reset: 0,
    state: () => ({ trades: 0, amt: 10, sold: {}, bought: {} }),
    slot: () => Math.floor(Date.now() / SLOT),
    spread() { return Math.max(0.02, 0.1 - 0.005 * ((G.S.city && G.S.city.lv.market) || 0)); },
    price(id) {
      const base = GOODS.find((g) => g[0] === id)[1];
      return Math.max(1, G.income() * base * mult(id, this.slot()));
    },
    // насыщение: объём сделок в «секундах дохода», затухает за ~10 минут
    base(id) { return GOODS.find((g) => g[0] === id)[1]; },
    buyP(id) { return this.price(id) * (1 + this.spread()) * (1 + (G.S.exchange.bought[id] || 0) / 300); },
    sellP(id) { return this.price(id) * (1 - this.spread()) / (1 + (G.S.exchange.sold[id] || 0) / 300); },
    tick(dt) {
      const s = G.S.exchange, k = Math.exp(-dt / 600);
      for (const id in s.sold) s.sold[id] *= k;
      for (const id in s.bought) s.bought[id] *= k;
    },
    // мини-график курса за последние 24 смены
    spark(id) {
      const now = this.slot(), pts = [];
      for (let i = 23; i >= 0; i--) pts.push(mult(id, now - i));
      const mn = Math.min(...pts), mx = Math.max(...pts);
      const d = pts.map((v, i) => `${(i * 100 / 23).toFixed(1)},${(28 - (v - mn) / (mx - mn || 1) * 26).toFixed(1)}`).join(' ');
      const up = pts[23] >= pts[22];
      return `<svg class="spark" viewBox="0 0 100 30" preserveAspectRatio="none"><polyline points="${d}" fill="none" stroke="${up ? '#45ff9a' : '#ff4d6d'}" stroke-width="2"/></svg>`;
    },
    panels: [{
      id: 'exchange', group: 'more', name: 'Биржа', icon: '📈', order: 44,
      unlock: () => G.has('mine') && (G.S.stats.prestiges || 0) >= 1,
      hint: 'Шахта и первый престиж',
      render() {
        const s = G.S.exchange;
        const left = (SLOT - (Date.now() % SLOT)) / 1000;
        let h = UI.info(`Курсы обновятся через <b>${Math.ceil(left)} с</b>. Спред ${U.pct(M.spread(), 1)}. Цены привязаны к вашему доходу.`);
        h += '<div class="chips small">' + [1, 10, 100, 1000].map((n) => `<button class="chip${s.amt === n ? ' on' : ''}" data-a="exchange.amt:${n}">×${n}</button>`).join('') + '</div>';
        for (const [id] of GOODS) {
          const c = G.CUR[id];
          if (!c) continue;
          const bp = M.buyP(id) * s.amt, sp = M.sellP(id) * s.amt;
          const m = mult(id, M.slot()), prev = mult(id, M.slot() - 1);
          h += `<div class="xrow${sel === id ? ' sel' : ''}" data-k="x${id}"><button class="xinfo" data-a="exchange.sel:${id}"><span class="xi">${c.icon}</span>
            <span><b>${c.name}</b><small>есть ${U.fint(G.cur(id))} · <span class="${m >= prev ? 'good' : 'bad'}">${m >= prev ? '▲' : '▼'} ${U.pct(Math.abs(m / prev - 1), 1)}</span></small></span></button>
            ${M.spark(id)}
            <div class="xbtns"><button class="mini${G.cur('coins') >= bp ? ' ok' : ''}" data-a="exchange.buy:${id}"${U.dis(G.cur('coins') >= bp)}>Купить<br>🪙${U.fmt(bp)}</button>
            <button class="mini${G.cur(id) >= s.amt ? ' ok' : ''}" data-a="exchange.sell:${id}"${U.dis(G.cur(id) >= s.amt)}>Продать<br>🪙${U.fmt(sp)}</button></div></div>`;
        }
        return h;
      },
    }],
    actions: {
      amt(n) { G.S.exchange.amt = +n; },
      sel(id) { sel = id; },
      buy(id) {
        const n = G.S.exchange.amt, c = M.buyP(id) * n;
        if (!G.pay({ coins: c })) { SFX.play('error'); return; }
        G.add(id, n); G.S.exchange.trades++; G.count('trades');
        G.S.exchange.bought[id] = (G.S.exchange.bought[id] || 0) + n * M.base(id);
        SFX.play('coin');
      },
      sell(id) {
        const n = G.S.exchange.amt;
        if (G.cur(id) < n) { SFX.play('error'); return; }
        G.add(id, -n);
        G.earn(M.sellP(id) * n, 'trade');
        G.S.exchange.sold[id] = (G.S.exchange.sold[id] || 0) + n * M.base(id);
        G.S.exchange.trades++; G.count('trades');
        SFX.play('coin');
      },
    },
  });
  void M;
})();
