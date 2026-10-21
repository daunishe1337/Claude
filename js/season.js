'use strict';
/* ============================================================
   season.js — сезонный пропуск.
   Сезон длится 30 дней. Опыт сезона — за квесты, боссов,
   престижи, события и задания сезона. 50 уровней бесплатных
   наград: души, яйца, свитки, паки карт, скины и реликвия.
   ============================================================ */
(() => {
  const LEN = 30;          // дней в сезоне
  const MAXL = 50;
  const xpNeed = (l) => 300 + 15 * l;
  // задания сезона: [текст, счётчик, цель, опыт]
  const TASKS = [
    ['Сделать {n} тапов', 'taps', 50000, 1500],
    ['Победить {n} боссов', 'bossWins', 60, 1500],
    ['Поймать {n} золотых объектов', 'golden', 80, 1200],
    ['Совершить {n} престижей', 'prestiges', 10, 2000],
    ['Открыть {n} сундуков', 'chests', 30, 1200],
    ['Выполнить {n} квестов', 'quests', 40, 2000],
    ['Собрать урожай {n} раз', 'harvests', 60, 1000],
    ['Пройти {n} этажей подземелья', 'dungeonFloors', 50, 1500],
  ];
  // награда уровня l
  function reward(l) {
    if (l === 10) return { bg: 'cosmos', souls: 100 };
    if (l === 25) return { skin: 'sun', eggs: 5 };
    if (l === 50) return { relic: 1, souls: 1000, eggs: 10 };
    const r = {};
    const k = l % 5;
    if (k === 0) { r.eggs = 2; r.stars = 2; } else if (k === 1) r.souls = 15 + l * 2; else if (k === 2) r.food = 100 + 10 * l; else if (k === 3) r.scrolls = 1; else r.pack = 1;
    return r;
  }
  const label = (r) => [r.souls && `👻${r.souls}`, r.eggs && `🥚${r.eggs}`, r.food && `🍖${r.food}`, r.scrolls && `📜${r.scrolls}`, r.stars && `⭐${r.stars}`, r.pack && '🃏', r.skin && '🎨скин', r.bg && '🖼️фон', r.relic && '🏺'].filter(Boolean).join(' ');

  const M = G.mod({
    id: 'season',
    reset: 9,
    state: () => ({ id: -1, xp: 0, lv: 0, claimed: {}, base: {}, tasks: {} }),
    cur: () => Math.floor(U.dayNum() / LEN),
    daysLeft() { return LEN - (U.dayNum() % LEN); },
    check() {
      const s = G.S.season;
      if (s.id !== this.cur()) {
        s.id = this.cur(); s.xp = 0; s.lv = 0; s.claimed = {}; s.tasks = {};
        s.base = {};
        for (const [, k] of TASKS) s.base[k] = G.S.stats[k] || 0;
      }
    },
    addXp(v) {
      const s = G.S.season;
      this.check();
      if (s.lv >= MAXL) return;
      s.xp += v;
      while (s.lv < MAXL && s.xp >= xpNeed(s.lv)) { s.xp -= xpNeed(s.lv); s.lv++; G.toast(`🎫 Уровень сезона ${s.lv}!`, 'good'); }
    },
    tprog(i) { const [, k] = TASKS[i]; return Math.max(0, (G.S.stats[k] || 0) - (G.S.season.base[k] || 0)); },
    init() { this.check(); },
    panels: [{
      id: 'season', group: 'more', name: 'Сезон', icon: '🎫', order: 14,
      unlock: () => G.has('quests'),
      hint: 'Откройте квесты',
      render() {
        M.check();
        const s = G.S.season;
        let h = `<div class="pcard"><div class="row"><b>🎫 Сезон ${s.id % 100 + 1}</b><span class="dim">осталось ${M.daysLeft()} дн.</span></div>
          <div class="row"><span>Уровень <b class="hl">${s.lv}/${MAXL}</b></span><span class="dim">${s.lv < MAXL ? `${s.xp}/${xpNeed(s.lv)}` : 'максимум'}</span></div>
          ${UI.bar(s.lv < MAXL ? s.xp / xpNeed(s.lv) : 1, 'violet')}
          <button class="btn gold wide" data-a="season.claimAll"${U.dis(Object.keys(s.claimed).length < s.lv)}>🎁 Забрать все награды</button></div>`;
        h += UI.sec('Задания сезона');
        TASKS.forEach(([text, , n, xp], i) => {
          const p = M.tprog(i), done = s.tasks[i];
          h += UI.card({ k: 'st' + i, icon: done ? '✅' : '🎯', title: text.replace('{n}', U.fmt(n)), desc: `+${xp} опыта сезона`,
            extra: done ? '' : UI.bar(p / n, 'green thin', `${U.fmt(Math.min(p, n))} / ${U.fmt(n)}`),
            btn: done ? '' : `<button class="buy${p >= n ? ' ok' : ''}" data-a="season.task:${i}"${U.dis(p >= n)}>Забрать</button>`, cls: done ? 'done' : p >= n ? 'can' : '' });
        });
        h += UI.sec('Награды');
        h += '<div class="slv">';
        for (let l = 1; l <= MAXL; l++) {
          const got = s.claimed[l], open = s.lv >= l, big = l === 10 || l === 25 || l === 50;
          h += `<button class="slvl${got ? ' got' : open ? ' open' : ''}${big ? ' big' : ''}" data-a="season.claim:${l}" data-k="sl${l}"${U.dis(open && !got)}><b>${l}</b><small>${label(reward(l))}</small></button>`;
        }
        return h + '</div>';
      },
      badge: () => Object.keys(G.S.season.claimed).length < G.S.season.lv || TASKS.some((t, i) => !G.S.season.tasks[i] && M.tprog(i) >= t[2]),
    }],
    actions: {
      claim(l) {
        const s = G.S.season;
        if (s.claimed[l] || s.lv < l) return;
        s.claimed[l] = 1;
        const r = reward(l);
        for (const k of ['souls', 'eggs', 'food', 'scrolls', 'stars']) if (r[k]) G.add(k, r[k]);
        if (r.pack && G.S.cards) G.S.cards.packs++;
        if (r.skin && G.byId.cosmetics) G.byId.cosmetics.give(r.skin);
        if (r.bg && G.byId.cosmetics) G.byId.cosmetics.give(r.bg);
        if (r.relic && G.byId.artifacts) G.byId.artifacts.giveRelic(null, 'Награда 50 уровня сезона');
        SFX.play('coin');
      },
      claimAll() { for (let l = 1; l <= G.S.season.lv; l++) M.actions.claim(l); },
      task(i) {
        const s = G.S.season;
        if (s.tasks[i] || M.tprog(i) < TASKS[i][2]) return;
        s.tasks[i] = 1;
        M.addXp(TASKS[i][3]);
        SFX.play('ach');
      },
    },
  });
  // источники опыта сезона
  G.on('questDone', (k) => M.addXp(k === 'd' ? 120 : 600));
  G.on('bossWin', () => M.addXp(15));
  G.on('prestige', (l) => M.addXp(150 * l));
  G.on('golden', () => M.addXp(5));
  G.on('daily', () => M.addXp(200));
  G.on('chest', () => M.addXp(10));
})();
