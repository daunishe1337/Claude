'use strict';
/* ============================================================
   achievements.js — достижения и титулы.
   • Достижения по категориям; каждое даёт +2% ко всему доходу,
     за полную категорию — особый бонус;
   • другие модули регистрируют свои достижения через G.achTier();
   • титулы открываются за достижения, надетый титул даёт бонус.
   ============================================================ */
(() => {
  const CATS = {};   // id -> {id, name, icon, bonus, st, list:[]}
  const ALL = [];    // все достижения
  const TITLES = []; // {id, name, icon, need, desc, st}

  // категория: бонус за полное выполнение (desc + функция статов)
  G.achCat = (id, name, icon, bonus, st) => { CATS[id] = { id, name, icon, bonus, st, list: [] }; };
  // одно достижение
  G.achAdd = (cat, id, name, desc, check, prog) => {
    const a = { cat, id, name, desc, check, prog };
    CATS[cat].list.push(a);
    ALL.push(a);
  };
  // серия достижений по порогам: getter() >= t
  G.achTier = (cat, key, names, getter, ts, label) => {
    ts.forEach((t, i) => G.achAdd(cat, key + i, names[i] || names[names.length - 1] + ' ' + (i + 1), label(t), () => getter() >= t, () => [getter(), t]));
  };
  G.titleAdd = (id, name, icon, need, desc, st) => TITLES.push({ id, name, icon, need, desc, st });

  const stat = (k) => () => G.S.stats[k] || 0;
  const F = U.fmt;

  /* ---------- Категории и достижения ---------- */
  G.achCat('taps', 'Тапы', '👆', 'Тап ×1.5', (a) => a.mul('tap', 1.5));
  G.achTier('taps', 'tp', ['Первые шаги', 'Разминка', 'Тапер', 'Мастер тапа', 'Тап-машина', 'Легенда тапа', 'Бог тапа'],
    stat('taps'), [100, 1e3, 1e4, 5e4, 2e5, 1e6, 5e6], (t) => `Сделать ${F(t)} тапов`);

  G.achCat('wealth', 'Богатство', '🪙', 'Весь доход ×1.25', (a) => a.mul('all', 1.25));
  G.achTier('wealth', 'wl', ['Копилка', 'Кошелёк', 'Сейф', 'Миллионер', 'Миллиардер', 'Триллионер', 'Магнат', 'Олигарх', 'Король денег', 'Император монет'],
    () => G.S.life.coins || 0, [1e3, 1e5, 1e7, 1e9, 1e12, 1e15, 1e18, 1e21, 1e24, 1e30], (t) => `Заработать ${F(t)} монет за всё время`);

  G.achCat('income', 'Доход', '⚡', 'Доход зданий ×1.5', (a) => a.mul('cps', 1.5));
  G.achTier('income', 'in', ['Ручеёк', 'Ручей', 'Река', 'Поток', 'Водопад', 'Океан денег', 'Цунами'],
    () => G.cps, [10, 1e3, 1e5, 1e7, 1e10, 1e13, 1e16], (t) => `Доход ${F(t)}/с`);

  G.achCat('build', 'Здания', '🏭', 'Цены зданий −5%', (a) => a.mul('cost', 0.95));
  G.achTier('build', 'bl', ['Застройщик', 'Прораб', 'Архитектор', 'Градостроитель', 'Мегаполис', 'Урбанист', 'Повелитель стройки'],
    () => G.byId.buildings.total(), [10, 50, 100, 250, 500, 1000, 2000], (t) => `Суммарно ${t} уровней зданий`);
  G.achAdd('build', 'ball', 'Полный комплект', 'Построить все 20 видов зданий', () => G.S.buildings.lv.every((l) => l > 0), () => [G.S.buildings.lv.filter((l) => l > 0).length, 20]);
  G.achAdd('build', 'b100', 'Сотня', 'Любое здание 100 уровня', () => G.S.buildings.lv.some((l) => l >= 100), () => [Math.max(...G.S.buildings.lv), 100]);

  G.achCat('power', 'Сила тапа', '💪', 'Линии тапа ×1.5', (a) => a.mul('tapLine', 1.5));
  G.achTier('power', 'pw', ['Крепкая рука', 'Железный кулак', 'Стальная хватка', 'Титан', 'Колосс'],
    () => G.byId.tap.levels(), [10, 50, 150, 400, 800], (t) => `Суммарно ${t} уровней силы тапа`);
  G.achTier('power', 'tv', ['Тяжёлый тап', 'Мощный тап', 'Сокрушительный тап'],
    () => G.tapVal, [1e3, 1e6, 1e9], (t) => `Доход ${F(t)} за тап`);

  G.achCat('crit', 'Криты', '🎯', '+2 к силе крита', (a) => a.add('critMul', 2));
  G.achTier('crit', 'cr', ['Меткий', 'Снайпер', 'Критоман', 'Бич удачи'], stat('crits'), [10, 100, 1000, 1e4], (t) => `${F(t)} критов`);
  G.achTier('crit', 'mc', ['Мега!', 'Мега-мега!', 'Гига!'], stat('megas'), [1, 25, 250], (t) => `${F(t)} мегакритов`);

  G.achCat('combo', 'Комбо', '🔥', '+0.5 с к угасанию комбо', (a) => a.add('comboDecay', 0.5));
  G.achTier('combo', 'cb', ['Серия', 'Разгон', 'Ураган', 'Неудержимый', 'Бесконечность', 'Транс'],
    stat('maxCombo'), [50, 100, 250, 500, 1000, 2000], (t) => `Комбо ${t}`);

  G.achCat('boost', 'Бустеры и навыки', '🚀', 'Бустеры +25% длительности', (a) => a.mul('boostDur', 1.25));
  G.achTier('boost', 'bo', ['Ускорение', 'Форсаж', 'Нитро', 'Гиперпрыжок'], stat('boosts'), [1, 10, 50, 200], (t) => `Активировать бустеры ${t} раз`);
  G.achTier('boost', 'sk', ['Ученик мага', 'Чародей', 'Архимаг'], stat('skills'), [1, 50, 250], (t) => `Применить навыки ${t} раз`);

  G.achCat('events', 'События', '✨', 'Награды событий ×1.5', (a) => a.mul('evReward', 1.5));
  G.achTier('events', 'gd', ['Золотоискатель', 'Ловец', 'Охотник за золотом', 'Золотая лихорадка'], stat('golden'), [1, 10, 50, 200], (t) => `Поймать ${t} золотых объектов`);
  G.achTier('events', 'ch', ['Кладоискатель', 'Расхититель'], stat('chests'), [1, 25], (t) => `Открыть ${t} сундуков`);
  G.achTier('events', 'mt', ['Астроном', 'Ловец звёзд'], stat('meteors'), [1, 25], (t) => `Поймать ${t} метеоритов`);

  G.achCat('boss', 'Боссы', '👹', 'Урон по боссам ×1.5', (a) => a.mul('bossDmg', 1.5));
  G.achTier('boss', 'bw', ['Победитель', 'Воитель', 'Истребитель', 'Охотник', 'Палач', 'Легенда арены'], stat('bossWins'), [1, 5, 10, 25, 50, 100], (t) => `Победить ${t} боссов`);
  G.achTier('boss', 'ms', ['Путник', 'Странник', 'Покоритель', 'Первопроходец'], stat('maxStage'), [100, 250, 400, 500], (t) => `Достичь этапа ${t}`);

  G.achCat('world', 'Миры', '🗺️', '+25% к душам', (a) => a.mul('souls', 1.25));
  G.achTier('world', 'wd', ['Турист', 'Путешественник', 'Исследователь', 'Картограф', 'Повелитель миров'], () => G.byId.stage.worlds(), [2, 4, 6, 8, 10], (t) => `Открыть ${t} миров`);

  G.achCat('auto', 'Автоматизация', '🤖', 'Авто-тапер ×1.5', (a) => a.mul('autoPow', 1.5));
  G.achTier('auto', 'fg', ['Помощник', 'Бригада', 'Армия пальцев', 'Тысячерукий'], () => G.S.autotap.fingers, [1, 5, 10, 15], (t) => `Купить ${t} пальцев`);
  G.achTier('auto', 'at', ['Автомат', 'Конвейер'], stat('autoTaps'), [1e4, 1e6], (t) => `${F(t)} авто-тапов`);

  /* ---------- Титулы ---------- */
  const achN = () => Object.keys(G.S.ach.got).length;
  G.titleAdd('novice', 'Новичок', '🐣', () => true, '+5% к тапу', (a) => a.mul('tap', 1.05));
  G.titleAdd('tapper', 'Тапер', '👆', () => (G.S.stats.taps || 0) >= 1e4, '+25% к тапу', (a) => a.mul('tap', 1.25));
  G.titleAdd('magnate', 'Магнат', '🎩', () => (G.S.life.coins || 0) >= 1e9, '+15% ко всему доходу', (a) => a.mul('all', 1.15));
  G.titleAdd('builder', 'Строитель', '🏗️', () => G.byId.buildings.total() >= 500, '+25% к доходу зданий', (a) => a.mul('cps', 1.25));
  G.titleAdd('critter', 'Критоман', '🎯', () => (G.S.stats.crits || 0) >= 1000, '+3% к шансу крита', (a) => a.add('critCh', 0.03));
  G.titleAdd('combo', 'Комбо-мастер', '🔥', () => (G.S.stats.maxCombo || 0) >= 1000, '+1 с к угасанию комбо', (a) => a.add('comboDecay', 1));
  G.titleAdd('hunter', 'Охотник на боссов', '🏹', () => (G.S.stats.bossWins || 0) >= 25, 'Урон по боссам ×1.5', (a) => a.mul('bossDmg', 1.5));
  G.titleAdd('traveler', 'Путешественник', '🧭', () => G.byId.stage.worlds() >= 6, '+25% к душам', (a) => a.mul('souls', 1.25));
  G.titleAdd('lucky', 'Ловец удачи', '🍀', () => (G.S.stats.golden || 0) >= 50, 'Награды событий ×1.5', (a) => a.mul('evReward', 1.5));
  G.titleAdd('fingers', 'Тысячерукий', '🖐️', () => G.S.autotap.fingers >= 15, 'Авто-тапер ×1.5', (a) => a.mul('autoPow', 1.5));
  G.titleAdd('collector', 'Коллекционер', '🏅', () => achN() >= 50, '+10% ко всему доходу', (a) => a.mul('all', 1.1));
  G.titleAdd('legend', 'Легенда', '👑', () => achN() >= 80, '+25% ко всему доходу', (a) => a.mul('all', 1.25));

  /* ---------- Модуль ---------- */
  const M = G.mod({
    id: 'ach',
    reset: 0,
    CATS, ALL, TITLES,
    state: () => ({ got: {}, seen: 0, title: 'novice', title2: '', view: 'list' }),
    slots: () => (G.byId.prestige && G.byId.prestige.uni() >= 3 ? 2 : 1),
    onReset(layer) { if (layer >= 9) G.S.ach.got = {}; },
    count: achN,
    catDone(c) { return c.list.length > 0 && c.list.every((a) => G.S.ach.got[a.id]); },
    stats(acc) {
      const n = achN();
      acc.mul('all', 1 + 0.02 * n * G.st.achPow);
      for (const id in CATS) if (this.catDone(CATS[id])) CATS[id].st(acc);
      const t = TITLES.find((x) => x.id === G.S.ach.title);
      if (t && t.need()) t.st(acc);
      const t2 = this.slots() > 1 && G.S.ach.title2 !== G.S.ach.title && TITLES.find((x) => x.id === G.S.ach.title2);
      if (t2 && t2.need()) t2.st(acc);
    },
    check() {
      const got = G.S.ach.got;
      let n = 0;
      for (const a of ALL) {
        if (got[a.id]) continue;
        let ok = false;
        try { ok = a.check(); } catch (e) { ok = false; }
        if (ok) {
          got[a.id] = Date.now();
          n++;
          if (n <= 3) G.toast(`🏆 Достижение: <b>${a.name}</b> · +2% к доходу`, 'ach');
          const c = CATS[a.cat];
          if (this.catDone(c)) G.toast(`🌟 Категория «${c.name}» завершена: ${c.bonus}!`, 'ach');
          G.emit('ach', a.id);
        }
      }
      if (n) { SFX.play('ach'); G.dirty = true; }
      for (const t of TITLES) {
        if (!G.S.flags['t:' + t.id] && t.need()) {
          G.S.flags['t:' + t.id] = 1;
          if (t.id !== 'novice') G.toast(`🎖️ Новый титул: <b>${t.icon} ${t.name}</b>`, 'ach');
        }
      }
    },
    panels: [
      {
        id: 'ach', group: 'more', name: 'Достижения', icon: '🏆', order: 10,
        unlock: () => achN() > 0,
        hint: 'Получите первое достижение',
        render() {
          const n = achN();
          G.S.ach.seen = n;
          let h = UI.info(`Получено <b>${n}/${ALL.length}</b> · бонус <b>+${U.pct(0.02 * n * G.st.achPow, 0)}</b> ко всему доходу`);
          for (const id in CATS) {
            const c = CATS[id];
            const done = c.list.filter((a) => G.S.ach.got[a.id]).length;
            h += UI.sec(`${c.icon} ${c.name} ${done}/${c.list.length}`, (M.catDone(c) ? '✅ ' : '🎁 ') + c.bonus);
            h += '<div class="achs">';
            for (const a of c.list) {
              const g = G.S.ach.got[a.id];
              let pr = '';
              if (!g && a.prog) { const [x, t] = a.prog(); pr = UI.bar(x / t, 'thin', `${U.fmt(Math.min(x, t))} / ${U.fmt(t)}`); }
              h += `<div class="ach${g ? ' got' : ''}" data-k="${a.id}"><div class="an">${g ? '🏆' : '🔒'} ${a.name}</div><div class="ad">${a.desc}</div>${pr}</div>`;
            }
            h += '</div>';
          }
          return h;
        },
        badge: () => achN() > (G.S.ach.seen || 0),
      },
      {
        id: 'titles', group: 'more', name: 'Титулы', icon: '🎖️', order: 11,
        unlock: () => TITLES.filter((t) => t.need()).length > 1,
        hint: 'Получите второй титул',
        render() {
          let h = UI.info('Надетый титул даёт бонус. Новые титулы открываются за достижения.');
          const two = M.slots() > 1;
          if (two) h += UI.info('🌌 Вселенная №4+: можно носить два титула.');
          for (const t of TITLES) {
            const open = t.need(), on = G.S.ach.title === t.id, on2 = two && G.S.ach.title2 === t.id;
            let btn = '';
            if (open) {
              btn = on ? '<button class="buy max" disabled>Надет</button>' : `<button class="buy ok" data-a="ach.wear:${t.id}">Надеть</button>`;
              if (two && !on) btn += on2 ? '<button class="buy max" disabled>Слот 2</button>' : `<button class="buy" data-a="ach.wear2:${t.id}">Слот 2</button>`;
            }
            h += UI.card({ k: 'ti' + t.id, icon: open ? t.icon : '🔒', title: t.name, desc: t.desc, btn, cls: on || on2 ? 'sel' : open ? '' : 'cant' });
          }
          return h;
        },
      },
    ],
    actions: {
      wear(id) { const t = TITLES.find((x) => x.id === id); if (t && t.need()) { G.S.ach.title = id; if (G.S.ach.title2 === id) G.S.ach.title2 = ''; SFX.play('buy'); } },
      wear2(id) { const t = TITLES.find((x) => x.id === id); if (t && t.need() && M.slots() > 1) { G.S.ach.title2 = id; SFX.play('buy'); } },
    },
  });
  G.defStat('achPow', 1);
  G.on('second', () => M.check());
})();
