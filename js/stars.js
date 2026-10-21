'use strict';
/* ============================================================
   stars.js — созвездия.
   Карта звёзд: каждая звезда покупается за ⭐ звёзды и даёт
   небольшой бонус, собранное созвездие — мощный бонус.
   Звёзды дают башня, метеориты, ночные события и вознесение.
   ============================================================ */
(() => {
  G.defCur('stars', 'Звёзды', '⭐', 9, true);
  // созвездие: точки [x, y] (0..100), связи по порядку, бонус звезды и полный бонус
  const CONS = [
    { id: 'aries', name: 'Овен', icon: '♈', pts: [[15, 30], [35, 20], [55, 28], [70, 45]], star: ['tap', 0.05], full: ['tap', 1], fdesc: 'Тап ×2' },
    { id: 'taurus', name: 'Телец', icon: '♉', pts: [[20, 60], [35, 45], [50, 50], [62, 35], [80, 25]], star: ['cps', 0.05], full: ['cps', 1], fdesc: 'Доход зданий ×2' },
    { id: 'gemini', name: 'Близнецы', icon: '♊', pts: [[25, 20], [25, 50], [25, 80], [55, 20], [55, 50], [55, 80]], star: ['critCh', 0.005], full: ['critMul', 3], fdesc: '+3 к силе крита' },
    { id: 'cancer', name: 'Рак', icon: '♋', pts: [[30, 30], [50, 45], [70, 30], [50, 70]], star: ['offline', 0.1], full: ['offlineH', 6], fdesc: '+6 ч офлайна' },
    { id: 'leo', name: 'Лев', icon: '♌', pts: [[20, 70], [30, 45], [45, 30], [65, 30], [75, 50], [60, 70]], star: ['all', 0.05], full: ['all', 1], fdesc: 'Весь доход ×2' },
    { id: 'virgo', name: 'Дева', icon: '♍', pts: [[20, 25], [35, 40], [50, 35], [60, 55], [75, 70]], star: ['farm', 0.1], full: ['farmSpeed', 0.5], fdesc: 'Рост растений +50%' },
    { id: 'libra', name: 'Весы', icon: '♎', pts: [[20, 60], [50, 30], [80, 60], [50, 75]], star: ['cost', 0.01], full: ['cost', 0.1], fdesc: 'Цены −10%' },
    { id: 'scorpio', name: 'Скорпион', icon: '♏', pts: [[15, 25], [30, 35], [45, 45], [55, 60], [65, 72], [80, 65], [85, 50]], star: ['bossDmg', 0.1], full: ['bossDmg', 2], fdesc: 'Урон по боссам ×3' },
  ];
  let sel = 0;
  const starCost = (ci, si) => (ci + 1) * (si + 1);

  const M = G.mod({
    id: 'stars',
    reset: 9,
    state: () => ({ got: {} }),
    has: (ci, si) => !!G.S.stars.got[ci + '_' + si],
    done(ci) { return CONS[ci].pts.every((_, si) => this.has(ci, si)); },
    stats(acc) {
      CONS.forEach((c, ci) => {
        let n = 0;
        c.pts.forEach((_, si) => { if (this.has(ci, si)) n++; });
        if (n) G.applyStat(acc, c.star[0], c.star[1] * n);
        if (n === c.pts.length) G.applyStat(acc, c.full[0], c.full[1]);
      });
    },
    levels() { return Object.keys(G.S.stars.got).length * 2; },
    panels: [{
      id: 'stars', group: 'more', name: 'Созвездия', icon: '✨', order: 50,
      unlock: () => (G.S.life.stars || 0) > 0,
      hint: 'Получите первую ⭐ (башня, метеориты, ночь)',
      render() {
        const c = CONS[sel];
        let h = `<div class="row"><span>⭐ Звёзды: <b>${U.fint(G.cur('stars'))}</b></span><span class="dim">собрано ${CONS.filter((_, i) => M.done(i)).length}/${CONS.length}</span></div>`;
        h += '<div class="chips small">' + CONS.map((x, i) => `<button class="chip${sel === i ? ' on' : ''}" data-a="stars.sel:${i}">${x.icon} ${x.name}${M.done(i) ? ' ✅' : ''}</button>`).join('') + '</div>';
        // карта
        let svg = '<svg class="skymap" viewBox="0 0 100 100">';
        for (let i = 1; i < c.pts.length; i++) {
          const [x1, y1] = c.pts[i - 1], [x2, y2] = c.pts[i];
          const on = M.has(sel, i - 1) && M.has(sel, i);
          svg += `<line x1="${x1}" y1="${y1}" x2="${x2}" y2="${y2}" stroke="${on ? '#ffd34d' : 'rgba(255,255,255,.18)'}" stroke-width="${on ? 1.2 : 0.6}"/>`;
        }
        c.pts.forEach(([x, y], si) => {
          const got = M.has(sel, si);
          svg += `<circle cx="${x}" cy="${y}" r="${got ? 3.6 : 2.6}" fill="${got ? '#ffd34d' : '#3a3a66'}" stroke="${got ? '#fff6c8' : '#8888bb'}" stroke-width="0.6" data-a="stars.buy:${sel}:${si}" class="sstar${got ? ' got' : ''}"/>`;
        });
        svg += '</svg>';
        h += `<div class="sky">${svg}</div>`;
        h += UI.info(`${c.icon} <b>${c.name}</b>: каждая звезда — ${G.statText(c.star[0], c.star[1])}. Всё созвездие: <span class="hl">${c.fdesc}</span>`);
        h += '<div class="minis">' + c.pts.map((_, si) => M.has(sel, si) ? `<span class="mini done">⭐${si + 1} ✔</span>`
          : `<button class="mini${G.cur('stars') >= starCost(sel, si) ? ' ok' : ''}" data-a="stars.buy:${sel}:${si}"${U.dis(G.cur('stars') >= starCost(sel, si))}>⭐${si + 1} · ${starCost(sel, si)} ⭐</button>`).join('') + '</div>';
        return h;
      },
      badge: () => CONS.some((c, ci) => c.pts.some((_, si) => !M.has(ci, si) && G.cur('stars') >= starCost(ci, si))),
    }],
    actions: {
      sel(i) { sel = +i; },
      buy(ci, si) {
        if (M.has(ci, si) || !G.pay({ stars: starCost(ci, si) })) { SFX.play('error'); return; }
        G.S.stars.got[ci + '_' + si] = 1;
        SFX.play('level');
        if (M.done(ci)) { SFX.play('win'); FX.flash('rgba(255,211,77,.4)'); G.toast(`✨ Созвездие «${CONS[ci].name}» собрано: ${CONS[ci].fdesc}!`, 'ach'); G.count('constellations'); }
      },
    },
  });
  // источники звёзд
  G.on('meteor', (loot) => { if (Math.random() < 0.5) { G.add('stars', 1); loot.list.push('⭐ 1'); } });
  G.on('golden', () => { const h = new Date().getHours(); if ((h >= 22 || h < 6) && Math.random() < 0.15) { G.add('stars', 1); G.toast('🌙 Ночная звезда! ⭐ +1'); } });
  G.on('prestige', (l) => { if (l >= 2) G.add('stars', 10); });
  G.achTier('research', 'cs', ['Звездочёт', 'Астролог', 'Хранитель неба'], () => G.S.stats.constellations || 0, [1, 4, 8], (t) => `Собрать ${t} созвездий`);
})();
