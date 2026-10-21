'use strict';
/* ============================================================
   quests.js — ежедневные и еженедельные квесты.
   Каждый день (по местному времени) выбираются задания из
   доступных систем; прогресс считается от счётчиков статистики
   на момент выдачи. Прокачка числа слотов — за души.
   ============================================================ */
(() => {
  // задание: [id, текст, счётчик статистики, цель (день), нужна открытая панель]
  const POOL = [
    ['taps', 'Сделать {n} тапов', 'taps', 800, null],
    ['buys', 'Купить {n} улучшений или зданий', 'buys', 60, null],
    ['golden', 'Поймать {n} золотых объекта', 'golden', 3, 'events'],
    ['boss', 'Победить {n} боссов', 'bossWins', 2, null],
    ['boosts', 'Активировать бустеры {n} раз', 'boosts', 3, 'boosters'],
    ['skills', 'Применить навыки {n} раз', 'skills', 6, 'skills'],
    ['streak', 'Сделать {n} серии по 100 тапов', 'streaks', 2, 'combo'],
    ['chests', 'Открыть {n} сундук', 'chests', 1, 'events'],
    ['crits', 'Нанести {n} критов', 'crits', 60, 'crit'],
    ['blocks', 'Разбить {n} блоков в шахте', 'blocks', 40, 'mine'],
    ['harvest', 'Собрать урожай {n} раз', 'harvests', 3, 'farm'],
    ['potion', 'Сварить {n} зелье', 'potions', 1, 'alchemy'],
    ['dungeon', 'Пройти {n} этажа подземелья', 'dungeonFloors', 3, 'dungeon'],
    ['hatch', 'Вылупить {n} питомца', 'hatched', 1, 'pets'],
    ['enchant', 'Зачаровать предметы {n} раза', 'enchants', 2, 'gear'],
    ['prestige', 'Совершить {n} престиж', 'prestiges', 1, 'p1'],
    ['trade', 'Совершить {n} сделки на бирже', 'trades', 3, 'exchange'],
    ['spin', 'Покрутить колесо удачи {n} раз', 'spins', 1, 'wheel'],
  ].map(([id, text, stat, n, need]) => ({ id, text, stat, n, need }));
  const BY = {};
  POOL.forEach((q) => (BY[q.id] = q));
  const weekNum = () => Math.floor((U.dayNum() + 3) / 7);

  const M = G.mod({
    id: 'quests',
    reset: 9,
    state: () => ({ day: -1, week: -1, d: [], w: [], slots: 0, done: 0 }),
    slotsN() { return 3 + G.S.quests.slots; },
    // выдать задания
    roll(kind) {
      const s = G.S.quests;
      const avail = POOL.filter((q) => !q.need || G.has(q.need));
      const seed = kind === 'd' ? U.dayNum() : weekNum() * 977;
      const rnd = U.seeded(seed * 31 + (G.S.t.created % 1000));
      const list = avail.slice().sort(() => rnd() - 0.5).slice(0, kind === 'd' ? this.slotsN() : 3);
      s[kind] = list.map((q) => ({ id: q.id, base: G.S.stats[q.stat] || 0, n: kind === 'd' ? q.n : q.n * 8, claimed: false }));
    },
    check() {
      const s = G.S.quests;
      if (s.day !== U.dayNum()) { s.day = U.dayNum(); this.roll('d'); }
      if (s.week !== weekNum()) { s.week = weekNum(); this.roll('w'); }
    },
    prog(q) { return Math.max(0, (G.S.stats[BY[q.id].stat] || 0) - q.base); },
    reward(kind) {
      const k = kind === 'd' ? 1 : 6;
      return { souls: Math.round(5 * k * Math.max(1, G.byId.stage.worlds() / 2)), food: 40 * k, eggs: kind === 'd' ? 0 : 2, scrolls: kind === 'd' ? 0 : 1 };
    },
    init() { this.check(); },
    panels: [{
      id: 'quests', group: 'more', name: 'Квесты', icon: '📜', order: 13,
      unlock: () => (G.S.stats.taps || 0) >= 300,
      hint: '300 тапов',
      render() {
        M.check();
        const s = G.S.quests;
        const toMidnight = (new Date().setHours(24, 0, 0, 0) - Date.now()) / 1000;
        let h = UI.info(`Ежедневные обновятся через <b>${U.time(toMidnight)}</b>. Выполнено всего: ${s.done}.`);
        const block = (kind, title) => {
          let x = UI.sec(title);
          s[kind].forEach((q, i) => {
            const d = BY[q.id];
            if (!d) return;
            const p = M.prog(q), ok = p >= q.n;
            const rw = M.reward(kind);
            x += UI.card({
              k: kind + q.id, icon: q.claimed ? '✅' : ok ? '🎁' : '📜', title: d.text.replace('{n}', U.fmt(q.n)),
              desc: Object.keys(rw).filter((r) => rw[r]).map((r) => G.CUR[r].icon + ' ' + rw[r]).join(' ') + ' · ⭐ опыт сезона',
              extra: q.claimed ? '' : UI.bar(p / q.n, 'green thin', `${U.fmt(Math.min(p, q.n))} / ${U.fmt(q.n)}`),
              btn: q.claimed ? '' : `<button class="buy${ok ? ' ok' : ''}" data-a="quests.claim:${kind}:${i}"${U.dis(ok)}>Забрать</button>`,
              cls: q.claimed ? 'done' : ok ? 'can' : '',
            });
          });
          return x;
        };
        h += block('d', 'Ежедневные');
        h += block('w', 'Еженедельные');
        if (s.slots < 3) {
          const c = 40 * Math.pow(3, s.slots);
          h += `<button class="btn wide" data-a="quests.slot"${U.dis(G.cur('souls') >= c)}>➕ Слот ежедневного квеста (${c} 👻)</button>`;
        }
        return h;
      },
      badge: () => ['d', 'w'].some((k) => G.S.quests[k].some((q) => !q.claimed && BY[q.id] && M.prog(q) >= q.n)),
    }],
    actions: {
      claim(kind, i) {
        const s = G.S.quests, q = s[kind][i];
        if (!q || q.claimed || M.prog(q) < q.n) return;
        q.claimed = true;
        const rw = M.reward(kind);
        for (const r in rw) if (rw[r]) G.add(r, rw[r]);
        s.done++;
        G.count('quests');
        G.emit('questDone', kind);
        SFX.play('ach');
      },
      slot() {
        const s = G.S.quests, c = 40 * Math.pow(3, s.slots);
        if (s.slots >= 3 || !G.pay({ souls: c })) { SFX.play('error'); return; }
        s.slots++;
        // добавить квест в новый слот сегодня
        const have = s.d.map((q) => q.id);
        const add = POOL.find((q) => !have.includes(q.id) && (!q.need || G.has(q.need)));
        if (add) s.d.push({ id: add.id, base: G.S.stats[add.stat] || 0, n: add.n, claimed: false });
        SFX.play('buy');
      },
    },
  });
  G.on('buy', (k, n) => G.count('buys', n || 1));
  G.on('second', () => { if (G.S) M.check(); });
  G.achCat('daily', 'Постоянство', '📅', 'Души +25%', (a) => a.mul('souls', 1.25));
  G.achTier('daily', 'qs', ['Исполнительный', 'Ответственный', 'Незаменимый'], () => G.S.stats.quests || 0, [1, 25, 150], (t) => `Выполнить ${t} квестов`);
})();
