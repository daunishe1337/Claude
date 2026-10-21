'use strict';
/* ============================================================
   tap.js — объект для тапа и «Сила тапа»:
   • мультитач: каждый палец — отдельный тап (pointerdown);
   • эффекты тапа: «+N», сжатие, частицы, звук, вибрация;
   • 25 линий улучшений: цена base × 1.15^уровень,
     множитель ×2 на 10/25/50/100 уровнях;
   • каждая 5-я линия добавляет к тапу % от дохода в секунду.
   ============================================================ */
(() => {
  const R = 1.15;
  const MILESTONES = [10, 25, 50, 100];
  // [название, иконка]; цена линии i = 10·11^i
  const NAMES = [
    ['Крепкий палец', '👆'], ['Мозоли', '✋'], ['Перчатка силы', '🧤'], ['Кастет', '🥊'], ['Чутьё дохода', '💡'],
    ['Стальные пальцы', '🦾'], ['Молот', '🔨'], ['Гром-кулак', '⚡'], ['Огненный тап', '🔥'], ['Деловая хватка', '💼'],
    ['Ледяной удар', '❄️'], ['Драконья лапа', '🐉'], ['Титановые костяшки', '🔩'], ['Рунный тап', '🔮'], ['Золотое касание', '👑'],
    ['Звёздный палец', '⭐'], ['Метеоритный удар', '☄️'], ['Плазменный импульс', '🌩️'], ['Квантовый тап', '⚛️'], ['Рука Мидаса', '🤲'],
    ['Касание бога', '🌟'], ['Гравитационный тап', '🕳️'], ['Тап сверхновой', '💥'], ['Палец вселенной', '🌌'], ['Абсолютный тап', '♾️'],
  ];
  // окупаемость здания с той же ценой (цена / доход) — для баланса
  const bldRatio = (c) => 150 * Math.pow(c / 15, 0.27);
  const LINES = NAMES.map(([name, icon], i) => {
    const cost = 10 * Math.pow(11, i);
    const share = i % 5 === 4;                    // линия «% от дохода/с»
    // первые линии очень выгодны (быстрый старт), дальше при 5 тапах/с
    // линия тапа окупается так же, как здание той же цены
    const ratio = Math.min(10 * Math.pow(4, i), 5 * bldRatio(cost));
    return {
      i, name, icon, cost, share,
      // сила за уровень: flat — монеты за тап; share — доля дохода/с
      pow: share ? 0.0002 * Math.pow(2, (i - 4) / 5) : cost / ratio,
      max: share ? 100 : Infinity,
    };
  });
  const msMult = (lv) => { let m = 1; for (const x of MILESTONES) if (lv >= x) m *= 2; return m; };
  const nextMs = (lv) => MILESTONES.find((x) => x > lv);

  const T = G.mod({
    id: 'tap',
    reset: 1,
    LINES, msMult,
    state: () => ({ lv: LINES.map(() => 0) }),

    // базовая сила тапа (без глобальных множителей)
    base() {
      const lv = G.S.tap.lv;
      let p = 1;
      for (const L of LINES) if (!L.share && lv[L.i]) p += L.pow * lv[L.i] * msMult(lv[L.i]) * G.st.tapLine;
      return p;
    },
    share() {
      const lv = G.S.tap.lv;
      let s = 0;
      for (const L of LINES) if (L.share && lv[L.i]) s += L.pow * lv[L.i] * msMult(lv[L.i]);
      return s;
    },
    stats(acc) { acc.add('tapCps', this.share()); },
    levels() { return G.S.tap.lv.reduce((a, b) => a + b, 0); },

    // сколько линий показывать: купленные + 2 следующих
    shown() {
      const lv = G.S.tap.lv;
      let last = -1;
      lv.forEach((v, i) => { if (v > 0) last = i; });
      return Math.min(LINES.length, last + 3);
    },
    offerOf(i) { return G.offer('t' + i, LINES[i].cost, R, G.S.tap.lv[i], LINES[i].max); },
    goals() {
      const n = this.shown();
      const out = [];
      for (let i = 0; i < n; i++) out.push({ icon: LINES[i].icon, name: LINES[i].name, cost: G.lvCost('t' + i, LINES[i].cost, R, G.S.tap.lv[i], 1) });
      return out;
    },

    // средняя частота ручных тапов за последние 5 секунд
    rate() {
      const now = performance.now();
      while (recent.length && now - recent[0] > 5000) recent.shift();
      return recent.length / 5;
    },

    panels: [{
      id: 'tapUp', group: 'upg', name: 'Сила тапа', icon: '👆', order: 1,
      render() {
        const lv = G.S.tap.lv;
        let h = UI.amt();
        h += UI.info(`Тап: <b>${U.fmt(G.tapVal)}</b> монет · база ${U.fmt(T.base())}${G.st.tapCps > 0 ? ` · +${U.pct(G.st.tapCps, 2)} дохода/с к тапу` : ''}`);
        const n = T.shown();
        for (let i = 0; i < n; i++) {
          const L = LINES[i], l = lv[i];
          const o = T.offerOf(i);
          const mm = msMult(l), nx = nextMs(l);
          const per = L.share ? `+${U.pct(L.pow * mm, 2)} дохода/с к тапу` : `+${U.fmt(L.pow * mm * G.st.tapLine)} к тапу`;
          const extra = nx ? UI.bar(l / nx, 'thin', `×2 на ур. ${nx}`) : '<div class="tag">все бонусы ×16</div>';
          h += UI.card({
            k: 't' + i, icon: L.icon, title: L.name, lv: l ? 'ур. ' + l : '',
            desc: `${per} за ур.${mm > 1 ? ` · <span class="hl">${U.mul(mm)}</span>` : ''}`,
            extra, btn: UI.buy('tap.buy:' + i, o), cls: o.ok ? 'can' : 'cant',
          });
        }
        if (n < LINES.length) h += UI.card({ k: 'tnext', icon: '❔', title: '???', desc: 'Купите предыдущее улучшение', cls: 'cant' });
        return h;
      },
      badge() { const n = T.shown(); for (let i = 0; i < n; i++) if (T.offerOf(i).ok && G.S.tap.lv[i] === 0) return true; return false; },
    }],

    actions: {
      buy(i) {
        const o = T.offerOf(i);
        if (!G.take(o)) { SFX.play('error'); return; }
        G.S.tap.lv[i] += o.n;
        G.emit('buy', 'tap', o.n);
        SFX.play('buy');
      },
    },

    init() { bindOrb(); },
  });
  G.defStat('tapLine', 1); // множитель силы линий тапа

  /* ---------- Обработка касаний ---------- */
  const recent = [];
  function bindOrb() {
    const orb = document.getElementById('orb');
    const zone = document.getElementById('tapZone');
    const press = (e) => {
      e.preventDefault();
      SFX.unlock();
      recent.push(performance.now());
      const ctx = G.doTap(e.clientX, e.clientY, false);
      tapFx(ctx, orb);
    };
    orb.addEventListener('pointerdown', press);
    // не даём странице скроллиться/зумиться от касаний зоны
    zone.addEventListener('touchstart', (e) => { if (e.target === zone || orb.contains(e.target)) e.preventDefault(); }, { passive: false });
    orb.addEventListener('contextmenu', (e) => e.preventDefault());
  }

  // эффекты одного тапа
  function tapFx(ctx, orb) {
    const big = ctx.crit >= 2, crit = ctx.crit >= 1;
    FX.float(ctx.x, ctx.y, (ctx.label ? ctx.label + ' ' : '') + '+' + U.fmt(ctx.total), ctx.color || (crit ? '#ffb020' : '#ffffff'), big ? 34 : crit ? 28 : 22);
    FX.burst(ctx.x, ctx.y, big ? 30 : crit ? 14 : 6, big ? ['#ff3df2', '#b44dff', '#ffffff'] : crit ? ['#ffb020', '#ffd34d', '#fff'] : ['#22e6ff', '#ffd34d', '#ff3df2']);
    if (big) { FX.shake(true); FX.ring(ctx.x, ctx.y, '#ff3df2', 200); SFX.play('mega'); VIB(40); }
    else if (crit) { FX.ring(ctx.x, ctx.y, '#ffb020', 120); SFX.play('crit'); VIB(20); }
    else { SFX.play('tap', G.byId.combo ? G.byId.combo.count() : 0); VIB(8); }
    orb.animate([{ transform: 'scale(0.9)' }, { transform: 'scale(1.03)' }, { transform: 'scale(1)' }], { duration: 160, easing: 'ease-out' });
  }
  T.tapFx = tapFx;
})();
