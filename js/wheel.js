'use strict';
/* ============================================================
   wheel.js — колесо удачи.
   Бесплатное вращение раз в несколько часов (копятся),
   8 секторов наград. Прокачка: частота, запас вращений, удача
   (шанс редких секторов). Прокачка — за души.
   ============================================================ */
(() => {
  // сектор: [id, подпись, иконка, вес, цвет]
  const SEG = [
    ['coinS', '5 мин дохода', '🪙', 20, '#2a6cff'],
    ['souls', 'Души', '👻', 14, '#7a3dff'],
    ['eggs', '2 яйца', '🥚', 12, '#e0a020'],
    ['food', 'Корм', '🍖', 14, '#d0503a'],
    ['coinL', '30 мин дохода', '💰', 10, '#1aa86a'],
    ['scroll', '2 свитка', '📜', 10, '#8a6a3a'],
    ['boost', 'Сброс бустеров', '🚀', 10, '#19a0c0'],
    ['jack', 'ДЖЕКПОТ', '🎰', 2, '#ff3df2'],
  ].map(([id, label, icon, w, c]) => ({ id, label, icon, w, c }));
  const UPG = {
    cd: { name: 'Часовщик', icon: '⏱️', max: 10, base: 15, r: 1.6, val: (l) => 'вращение раз в ' + U.time(4 * 3600 * Math.pow(0.93, l)) },
    store: { name: 'Копилка', icon: '🫙', max: 4, base: 25, r: 2, val: (l) => 'запас ' + (1 + l) },
    luck: { name: 'Фортуна', icon: '🍀', max: 10, base: 20, r: 1.7, val: (l) => 'редкие сектора ×' + (1 + 0.25 * l).toFixed(2) },
  };
  let angle = 0, spinning = false;

  const M = G.mod({
    id: 'wheel',
    reset: 9,
    state: () => ({ spins: 1, at: Date.now(), cd: 0, store: 0, luck: 0, total: 0 }),
    period() { return 4 * 3600000 * Math.pow(0.93, G.S.wheel.cd); },
    maxSpins() { return 1 + G.S.wheel.store; },
    regen() {
      const s = G.S.wheel;
      if (s.spins >= this.maxSpins()) { s.at = Date.now(); return; }
      while (s.spins < this.maxSpins() && Date.now() - s.at >= this.period()) { s.spins++; s.at += this.period(); }
    },
    tick() { this.regen(); },
    offline() { this.regen(); },
    weight(seg) { return seg.w * (seg.id === 'jack' || seg.id === 'coinL' || seg.id === 'eggs' ? 1 + 0.25 * G.S.wheel.luck : 1); },
    levels() { const s = G.S.wheel; return s.cd + s.store + s.luck; },
    grant(seg) {
      const ev = G.byId.events;
      switch (seg.id) {
        case 'coinS': { const v = ev.secs(300); G.earn(v, 'event'); return `🪙 ${U.fmt(v)}`; }
        case 'coinL': { const v = ev.secs(1800); G.earn(v, 'event'); return `💰 ${U.fmt(v)}`; }
        case 'souls': { const v = Math.round(10 * G.byId.stage.worlds() * G.st.souls); G.add('souls', v); return `👻 ${v}`; }
        case 'eggs': G.add('eggs', 2); return '🥚 2';
        case 'food': G.add('food', 150); return '🍖 150';
        case 'scroll': G.add('scrolls', 2); return '📜 2';
        case 'boost': { const b = G.S.boosters; for (const k in b.ready) b.ready[k] = 0; return '🚀 бустеры готовы'; }
        case 'jack': {
          const v = ev.secs(7200); G.earn(v, 'event'); G.add('eggs', 5); G.add('souls', 50);
          if (Math.random() < 0.05 && G.byId.artifacts) setTimeout(() => G.byId.artifacts.giveRelic(null, 'Джекпот колеса удачи'), 400);
          FX.flash('rgba(255,61,242,.45)');
          return `🎰 ДЖЕКПОТ: 🪙 ${U.fmt(v)} · 🥚 5 · 👻 50`;
        }
      }
      return '';
    },
    // SVG колеса
    svg() {
      const n = SEG.length, R = 48;
      let h = `<svg viewBox="-50 -50 100 100" class="wsvg" style="transform:rotate(${angle}deg)">`;
      SEG.forEach((s, i) => {
        const a0 = (i / n) * Math.PI * 2 - Math.PI / 2, a1 = ((i + 1) / n) * Math.PI * 2 - Math.PI / 2;
        const x0 = R * Math.cos(a0), y0 = R * Math.sin(a0), x1 = R * Math.cos(a1), y1 = R * Math.sin(a1);
        const am = (a0 + a1) / 2;
        h += `<path d="M0 0 L${x0.toFixed(2)} ${y0.toFixed(2)} A${R} ${R} 0 0 1 ${x1.toFixed(2)} ${y1.toFixed(2)} Z" fill="${s.c}" stroke="#0b0b20" stroke-width="0.8"/>`;
        h += `<text x="${(32 * Math.cos(am)).toFixed(2)}" y="${(32 * Math.sin(am)).toFixed(2)}" font-size="11" text-anchor="middle" dominant-baseline="central">${s.icon}</text>`;
      });
      return h + '<circle r="7" fill="#15153a" stroke="#ffd34d" stroke-width="1.5"/></svg>';
    },
    panels: [{
      id: 'wheel', group: 'more', name: 'Колесо удачи', icon: '🎡', order: 12,
      unlock: () => (G.S.life.coins || 0) >= 1e6,
      hint: '1M монет за всё время',
      render() {
        const s = G.S.wheel;
        const next = s.spins < M.maxSpins() ? U.time((M.period() - (Date.now() - s.at)) / 1000) : 'запас полон';
        let h = `<div class="wheel"><div class="wptr">▼</div><div class="wwrap" data-static>${M.svg()}</div></div>`;
        h += `<div class="center">Вращений: <b class="hl">${s.spins}/${M.maxSpins()}</b> · следующее: ${next}</div>`;
        h += `<button class="btn gold wide" data-a="wheel.spin"${U.dis(s.spins > 0 && !spinning)}>🎡 Крутить!</button>`;
        h += UI.sec('Улучшения', 'за души');
        for (const k in UPG) {
          const d = UPG[k], l = s[k], max = l >= d.max, c = Math.round(d.base * Math.pow(d.r, l));
          h += UI.card({ k: 'w' + k, icon: d.icon, title: d.name, lv: 'ур. ' + l, desc: `${d.val(l)}${max ? '' : ` → <span class="hl">${d.val(l + 1)}</span>`}`,
            btn: max ? '<button class="buy max" disabled>МАКС</button>' : UI.buyC('wheel.up:' + k, { souls: c }), cls: max ? 'done' : G.cur('souls') >= c ? 'can' : 'cant' });
        }
        return h;
      },
      badge: () => G.S.wheel.spins > 0,
    }],
    actions: {
      spin() {
        const s = G.S.wheel;
        if (spinning || s.spins < 1) return;
        s.spins--;
        if (s.spins === M.maxSpins() - 1) s.at = Date.now();
        s.total++;
        G.count('spins');
        const seg = U.wpick(SEG, (x) => M.weight(x));
        const i = SEG.indexOf(seg), n = SEG.length;
        // поворот так, чтобы сектор i оказался под стрелкой (вверху)
        const target = 360 - (i + 0.5) * (360 / n);
        angle = angle - (angle % 360) + 360 * 5 + target;
        spinning = true;
        const svg = document.querySelector('.wsvg');
        if (svg) svg.style.transform = `rotate(${angle}deg)`;
        SFX.play('open');
        setTimeout(() => {
          spinning = false;
          const txt = M.grant(seg);
          SFX.play(seg.id === 'jack' ? 'win' : 'coin');
          UI.modal({ title: `${seg.icon} ${seg.label}`, html: `<div class="center big" style="font-size:20px">${txt}</div>`, buttons: [{ text: 'Забрать', cls: 'gold' }] });
          G.dirty = true;
        }, 3700);
      },
      up(k) {
        const s = G.S.wheel, d = UPG[k], c = Math.round(d.base * Math.pow(d.r, s[k]));
        if (s[k] >= d.max || !G.pay({ souls: c })) { SFX.play('error'); return; }
        s[k]++;
        SFX.play('buy');
      },
    },
  });
  void M;
})();
