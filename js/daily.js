'use strict';
/* ============================================================
   daily.js — ежедневная награда.
   Забирается раз в календарный день; серия растёт, если заходить
   каждый день (пропуск — серия начинается заново).
   Особые награды на 7-й, 30-й и 100-й день серии.
   ============================================================ */
(() => {
  // награда за n-й день серии
  function reward(n) {
    const r = { souls: 5 + n, food: 20 + 5 * n };
    if (n % 3 === 0) r.eggs = 1;
    if (n % 5 === 0) r.scrolls = 1;
    if (n % 7 === 0) { r.eggs = (r.eggs || 0) + 3; r.souls += 50; r.stars = 2; }
    if (n === 30) { r.eggs += 10; r.skin = 'dice'; }
    if (n === 100) { r.skin = 'crown'; r.relic = 1; r.souls += 500; }
    return r;
  }
  const label = (r) => Object.keys(r).filter((k) => G.CUR[k]).map((k) => `${G.CUR[k].icon} ${r[k]}`).join(' ')
    + (r.skin ? ' · 🎨 скин' : '') + (r.relic ? ' · 🏺 реликвия' : '');

  const M = G.mod({
    id: 'daily',
    reset: 9,
    state: () => ({ last: -1, streak: 0, best: 0, total: 0 }),
    can() { return G.S.daily.last !== U.dayNum(); },
    nextDay() { const s = G.S.daily; return s.last === U.dayNum() - 1 ? s.streak + 1 : 1; },
    panels: [{
      id: 'daily', group: 'more', name: 'Награда дня', icon: '🎁', order: 1,
      render() {
        const s = G.S.daily, can = M.can(), n = can ? M.nextDay() : s.streak;
        let h = `<div class="pcard"><div class="center" style="font-size:46px">🎁</div>
          <div class="center">Серия: <b class="hl">${s.streak} дн.</b> · рекорд ${s.best} · всего ${s.total}</div>
          ${can ? `<div class="center">Сегодня (день ${n}): <b>${label(reward(n))}</b></div><button class="btn gold wide" data-a="daily.claim">Забрать награду</button>`
            : '<div class="center dim">Сегодняшняя награда получена. Возвращайтесь завтра!</div>'}</div>`;
        h += UI.sec('Ближайшие дни');
        const start = can ? n : n + 1;
        h += '<div class="cal">';
        for (let d = start; d < start + 7; d++) {
          const r = reward(d), special = d % 7 === 0 || d === 30 || d === 100;
          h += `<div class="calday${special ? ' sp' : ''}${d === start && can ? ' now' : ''}" data-k="d${d}"><b>${d}</b><small>${r.skin ? '🎨' : r.relic ? '🏺' : r.eggs ? '🥚' : '👻'}</small></div>`;
        }
        h += '</div>';
        h += UI.info('Особые награды: каждый 7-й день — яйца и звёзды, 30-й день — скин «Кубик удачи», 100-й день — «Корона» и реликвия. Пропуск дня обнуляет серию.', 'dim');
        return h;
      },
      badge: () => M.can(),
    }],
    actions: {
      claim() {
        const s = G.S.daily;
        if (!M.can()) return;
        const n = M.nextDay();
        s.streak = n; s.last = U.dayNum(); s.total++;
        s.best = Math.max(s.best, n);
        const r = reward(n);
        for (const k in r) if (G.CUR[k]) G.add(k, r[k]);
        if (r.skin && G.byId.cosmetics) G.byId.cosmetics.give(r.skin);
        if (r.relic && G.byId.artifacts) G.byId.artifacts.giveRelic(null, '100 дней подряд!');
        G.count('dailies');
        SFX.play('win');
        FX.flash('rgba(255,211,77,.35)');
        G.toast(`🎁 День ${n}: ${label(r)}`, 'ach');
        G.emit('daily', n);
      },
    },
  });
  // напоминание при входе
  G.on('boot', () => { if (M.can()) setTimeout(() => G.toast('🎁 Ежедневная награда ждёт! (Ещё → Награда дня)', 'unlock'), 1500); });
  G.achTier('daily', 'dl', ['Неделя', 'Месяц', 'Сотня'], () => G.S.daily.best, [7, 30, 100], (t) => `Серия наград ${t} дней`);
})();
