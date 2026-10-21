'use strict';
/* ============================================================
   mine.js — шахта.
   Этажи вглубь: каждый блок нужно разбить (тапами или
   автошахтёрами). Ресурсы: дерево, камень, металл, самоцветы,
   эфир — чем глубже, тем ценнее. Прокачка кирки, скорости,
   вместимости тележки и найм автошахтёров.
   ============================================================ */
(() => {
  G.defCur('wood', 'Дерево', '🪵', 3);
  G.defCur('stone', 'Камень', '🪨', 3);
  G.defCur('metal', 'Металл', '🔩', 3);
  G.defCur('gems', 'Самоцветы', '💠', 3);
  G.defCur('ether', 'Эфир', '🌀', 3);
  // ресурс: с какого этажа появляется и базовая добыча с блока
  const RES = [['wood', 1, 4], ['stone', 3, 3], ['metal', 10, 2], ['gems', 25, 1], ['ether', 50, 0.3]];
  const BLOCKS = 10; // блоков на этаже
  const UPG = {
    pick: { name: 'Кирка', icon: '⛏️', max: 100, desc: 'Урон по блоку (тап и шахтёры)', cost: (l) => ({ wood: Math.round(15 * Math.pow(1.45, l)), stone: Math.round(8 * Math.pow(1.5, l)) }), val: (l) => U.fmt(M.pickPow(l)) },
    speed: { name: 'Скорость', icon: '💨', max: 50, desc: 'Ударов в секунду у шахтёров', cost: (l) => ({ wood: Math.round(30 * Math.pow(1.5, l)), metal: Math.round(4 * Math.pow(1.55, l)) }), val: (l) => (0.5 + 0.1 * l).toFixed(1) + '/с' },
    cap: { name: 'Вместимость', icon: '🛒', max: 50, desc: 'Сколько добычи копит тележка', cost: (l) => ({ stone: Math.round(25 * Math.pow(1.5, l)) }), val: (l) => U.fmt(M.capOf(l)) },
    miner: { name: 'Автошахтёр', icon: '👷', max: 30, desc: 'Сам копает, даже офлайн', cost: (l) => ({ souls: Math.round(5 * Math.pow(1.6, l)), wood: Math.round(50 * Math.pow(1.4, l)) }), val: (l) => l + ' чел.' },
  };

  const M = G.mod({
    id: 'mine',
    reset: 3,
    UPG,
    state: () => ({ floor: 1, block: 0, hp: 0, pick: 0, speed: 0, cap: 0, miner: 0, cart: {}, maxFloor: 1 }),
    pickPow: (l) => (1 + l) * Math.pow(1.08, l) * G.st.minePow,
    capOf: (l) => Math.round(200 * (1 + 0.5 * l)),
    blockHp(f) { return 20 * Math.pow(1.12, f - 1); },
    // добыча с одного блока
    yieldOf(f) {
      const out = {};
      for (const [id, from, base] of RES) if (f >= from) out[id] = base * (1 + 0.08 * (f - from)) * G.st.mine;
      return out;
    },
    dps() { const s = G.S.mine; return s.miner * (0.5 + 0.1 * s.speed) * this.pickPow(s.pick); },
    cartTotal() { let n = 0; for (const k in G.S.mine.cart) n += G.S.mine.cart[k]; return n; },
    init() { if (!G.S.mine.hp) G.S.mine.hp = this.blockHp(G.S.mine.floor); },
    // удар по блоку: auto — урон шахтёров (добыча в тележку)
    hit(dmg, auto) {
      const s = G.S.mine;
      let broke = 0, guard = 0;
      while (dmg > 0 && guard++ < 500) {
        if (dmg < s.hp) { s.hp -= dmg; break; }
        dmg -= s.hp;
        broke++;
        const y = this.yieldOf(s.floor);
        for (const k in y) {
          if (auto) {
            if (this.cartTotal() >= this.capOf(s.cap)) break;
            s.cart[k] = (s.cart[k] || 0) + y[k];
          } else G.add(k, y[k]);
        }
        s.block++;
        if (s.block >= BLOCKS) { s.block = 0; s.floor++; if (s.floor > s.maxFloor) { s.maxFloor = s.floor; G.best('mineFloor', s.floor); } }
        s.hp = this.blockHp(s.floor);
        if (auto && this.cartTotal() >= this.capOf(s.cap)) break;
      }
      if (broke) G.count('blocks', broke);
      return broke;
    },
    tick(dt) {
      if (!G.has('mine') || !G.S.mine.miner) return;
      if (this.cartTotal() < this.capOf(G.S.mine.cap)) this.hit(this.dps() * dt, true);
    },
    offline(sec) {
      if (!G.has('mine') || !G.S.mine.miner) return;
      // ограничиваем объём симуляции: ударов не больше, чем вмещает тележка
      this.hit(this.dps() * Math.min(sec, 86400), true);
    },
    levels() { const s = G.S.mine; return s.pick + s.speed + s.cap + s.miner + Math.floor(s.maxFloor / 5); },
    stats(acc) { acc.mul('all', 1 + 0.01 * (G.S.mine.maxFloor - 1)); },  // +1% за каждый этаж-рекорд
    panels: [{
      id: 'mine', group: 'more', name: 'Шахта', icon: '⛏️', order: 40,
      unlock: () => G.S.hero.lv >= 10 || (G.S.stats.prestiges || 0) >= 1,
      hint: 'Уровень героя 10 или первый престиж',
      render() {
        const s = G.S.mine, hp = M.blockHp(s.floor);
        const y = M.yieldOf(s.floor);
        let h = `<div class="pcard mine"><div class="row"><b>⛏️ Этаж ${s.floor}</b><span class="dim">блок ${s.block + 1}/${BLOCKS} · рекорд ${s.maxFloor} (+${s.maxFloor - 1}% к доходу)</span></div>
          <button class="mblock" data-a="mine.dig"><span>${s.floor >= 50 ? '🟪' : s.floor >= 25 ? '🟦' : s.floor >= 10 ? '⬛' : s.floor >= 3 ? '🟫' : '🟩'}</span></button>
          ${UI.bar(s.hp / hp, 'gold', `${U.fmt(s.hp)} / ${U.fmt(hp)}`)}
          <div class="center dim small">Тап: −${U.fmt(M.pickPow(s.pick))} · Блок даёт: ${Object.keys(y).map((k) => G.CUR[k].icon + ' ' + U.fmt(y[k])).join(' ')}</div></div>`;
        h += '<div class="resrow">' + RES.map(([id, from]) => `<span class="${s.maxFloor >= from ? '' : 'dim'}">${G.CUR[id].icon} ${U.fint(G.cur(id))}</span>`).join('') + '</div>';
        if (s.miner) {
          const tot = M.cartTotal(), cap = M.capOf(s.cap);
          h += UI.card({ k: 'cart', icon: '🛒', title: 'Тележка шахтёров', desc: `${s.miner} 👷 · ${U.fmt(M.dps())} урона/с`,
            extra: UI.bar(tot / cap, 'green thin', `${U.fmt(tot)} / ${U.fmt(cap)}`),
            btn: `<button class="buy${tot > 0 ? ' ok' : ''}" data-a="mine.collect"${U.dis(tot > 0)}>Собрать</button>` });
        }
        h += UI.sec('Улучшения шахты');
        for (const k in UPG) {
          const d = UPG[k], l = s[k], max = l >= d.max, c = d.cost(l);
          h += UI.card({ k: 'mu' + k, icon: d.icon, title: d.name, lv: `ур. ${l}`, desc: `${d.desc}<br>${d.val(l)}${max ? '' : ` → <span class="hl">${d.val(l + 1)}</span>`}`,
            btn: max ? '<button class="buy max" disabled>МАКС</button>' : UI.buyC('mine.up:' + k, c), cls: max ? 'done' : G.can(c) ? 'can' : 'cant' });
        }
        return h;
      },
      badge: () => M.cartTotal() >= M.capOf(G.S.mine.cap) * 0.9 && G.S.mine.miner > 0,
    }],
    actions: {
      dig(node) {
        const n = M.hit(M.pickPow(G.S.mine.pick), false);
        SFX.play(n ? 'coin' : 'tap', 10);
        VIB(n ? 15 : 6);
        if (node && node.getBoundingClientRect) {
          const r = node.getBoundingClientRect();
          FX.burst(r.left + r.width / 2, r.top + r.height / 2, n ? 14 : 5, ['#c98a3a', '#9a9ab0', '#ffd34d']);
        }
      },
      collect() {
        const s = G.S.mine;
        const got = [];
        for (const k in s.cart) { G.add(k, s.cart[k]); got.push(G.CUR[k].icon + ' ' + U.fmt(s.cart[k])); }
        s.cart = {};
        SFX.play('coin');
        if (got.length) G.toast('🛒 ' + got.join(' · '), 'good');
      },
      up(k) {
        const s = G.S.mine, d = UPG[k];
        if (s[k] >= d.max || !G.pay(d.cost(s[k]))) { SFX.play('error'); return; }
        s[k]++;
        SFX.play('buy');
        G.emit('buy', 'mine', 1);
      },
    },
  });
  G.defStat('minePow', 1);
  // метеорит приносит ресурсы шахты
  G.on('meteor', (loot) => {
    if (!G.has('mine')) return;
    const f = Math.max(1, G.S.mine.maxFloor);
    const y = M.yieldOf(f);
    for (const k in y) { const v = y[k] * 5; G.add(k, v); loot.list.push(G.CUR[k].icon + ' ' + U.fmt(v)); }
  });
  G.achCat('mine', 'Шахта', '⛏️', 'Добыча шахты ×2', (a) => a.mul('mine', 2));
  G.achTier('mine', 'mf', ['Шахтёр', 'Проходчик', 'Глубинник', 'Покоритель недр'], () => G.S.mine.maxFloor, [5, 25, 50, 100], (t) => `Этаж шахты ${t}`);
  G.achTier('mine', 'mb', ['Каменотёс', 'Горнорабочий'], () => G.S.stats.blocks || 0, [100, 5000], (t) => `Разбить ${t} блоков`);
})();
