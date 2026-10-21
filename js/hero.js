'use strict';
/* ============================================================
   hero.js — герой: уровень, опыт, характеристики и классы.
   • Опыт за тапы, покупки, боссов, события, достижения, престиж;
   • каждый уровень: +5 очков характеристик (Сила, Ловкость,
     Удача, Интеллект, Выносливость) и +1 очко класса;
   • 5 классов (Воин, Маг, Торговец, Охотник, Алхимик), у каждого
     дерево из 20 узлов; класс можно сменить.
   ============================================================ */
(() => {
  // характеристики героя: [название, иконка, описание за очко, эффект]
  const STATS = {
    str: ['Сила', '💪', 'Тап +4%, боевая сила +2%', (a, v) => { a.mul('tap', 1 + 0.04 * v); a.mul('combat', 1 + 0.02 * v); }],
    agi: ['Ловкость', '🤸', 'Шанс крита +0.1%, авто-тап +2%', (a, v) => { a.add('critCh', 0.001 * v); a.mul('autoSpeed', 1 + 0.02 * v); }],
    luk: ['Удача', '🍀', 'Награды событий +2%, добыча +1%', (a, v) => { a.mul('evReward', 1 + 0.02 * v); a.mul('drop', 1 + 0.01 * v); }],
    int: ['Интеллект', '🧠', 'Навыки +2%, мана +1, исследования +1%', (a, v) => { a.mul('skillPow', 1 + 0.02 * v); a.add('manaMax', v); a.mul('research', 1 + 0.01 * v); }],
    end: ['Выносливость', '🛡️', 'Бустеры +1%, офлайн +2%, здоровье +3%', (a, v) => { a.mul('boostDur', 1 + 0.01 * v); a.mul('offline', 1 + 0.02 * v); a.mul('combatHp', 1 + 0.03 * v); }],
  };

  // классы: базовый бонус и 5 рядов по 4 узла [название, иконка, стат, значение за уровень]
  const CLASSES = [
    { id: 'warrior', name: 'Воин', icon: '⚔️', role: 'тапы', desc: 'Тап ×1.5, боевая сила +25%',
      base: (a) => { a.mul('tap', 1.5); a.mul('combat', 1.25); },
      rows: [
        [['Мощный удар', '👊', 'tap', 0.1], ['Точность', '🎯', 'critCh', 0.004], ['Ярость', '😡', 'critMul', 0.2], ['Натиск', '🔥', 'comboLen', 5]],
        [['Боевой клич', '📯', 'tap', 0.15], ['Добивание', '💀', 'megaCh', 0.01], ['Стойкость', '🛡️', 'combatHp', 0.1], ['Темп', '⏩', 'comboDecay', 0.1]],
        [['Берсерк', '🪓', 'tap', 0.25], ['Сокрушение', '🔨', 'megaMul', 2], ['Вихрь клинков', '🌪️', 'autoPow', 0.1], ['Боевой дух', '💪', 'combat', 0.1]],
        [['Жатва', '🩸', 'tapCps', 0.004], ['Остриё', '🗡️', 'critCh', 0.008], ['Сила гиганта', '🗿', 'tap', 0.4], ['Неудержимость', '🐂', 'comboPow', 0.1]],
        [['Легенда арены', '🏆', 'tap', 1], ['Палач', '⚔️', 'bossDmg', 0.2], ['Гнев богов', '⚡', 'critMul', 1], ['Аватар войны', '👹', 'all', 0.1]],
      ] },
    { id: 'mage', name: 'Маг', icon: '🔮', role: 'навыки', desc: 'Навыки ×1.5, +50 маны',
      base: (a) => { a.mul('skillPow', 1.5); a.add('manaMax', 50); },
      rows: [
        [['Искра', '✨', 'skillPow', 0.1], ['Медитация', '🧘', 'manaRegen', 0.1], ['Резерв', '🫙', 'manaMax', 10], ['Концентрация', '🎐', 'skillCd', 0.03]],
        [['Огненный шар', '🔥', 'skillPow', 0.15], ['Поток маны', '🌊', 'manaRegen', 0.15], ['Мудрость', '📘', 'xp', 0.1], ['Ускорение', '⏩', 'skillCd', 0.03]],
        [['Метеор', '☄️', 'skillPow', 0.25], ['Колодец', '🪣', 'manaMax', 25], ['Знание', '📚', 'research', 0.1], ['Чары удачи', '🍀', 'evReward', 0.1]],
        [['Арканист', '🌀', 'skillPow', 0.4], ['Ночной дозор', '🌙', 'manaRegen', 0.25], ['Временная петля', '⏳', 'skillCd', 0.04], ['Кристальный разум', '💎', 'crystal', 0.05]],
        [['Архимаг', '🧙', 'skillPow', 1], ['Бесконечная мана', '♾️', 'manaRegen', 0.5], ['Звёздная магия', '✨', 'dust', 0.05], ['Повелитель стихий', '🌈', 'all', 0.1]],
      ] },
    { id: 'merchant', name: 'Торговец', icon: '💰', role: 'скидки', desc: 'Цены −10%, доход зданий +25%',
      base: (a) => { a.mul('cost', 0.9); a.mul('cps', 1.25); },
      rows: [
        [['Торг', '🏷️', 'cost', 0.01], ['Инвестиции', '📈', 'cps', 0.1], ['Удачная сделка', '🤝', 'evReward', 0.1], ['Склад', '📦', 'offline', 0.1]],
        [['Оптовик', '🏪', 'cost', 0.015], ['Монополия', '🏢', 'cps', 0.15], ['Синергия', '🔗', 'synergy', 0.1], ['Сбор налогов', '💼', 'offline', 0.15]],
        [['Скидочная карта', '💳', 'cost', 0.02], ['Акционер', '📊', 'cps', 0.25], ['Мастерство', '🏅', 'masteryK', 0.1], ['Богатый урожай', '🌾', 'farm', 0.1]],
        [['Барон', '🎩', 'cps', 0.4], ['Ростовщик', '🏦', 'all', 0.05], ['Кристальный рынок', '💎', 'crystal', 0.05], ['Торговые пути', '🗺️', 'drop', 0.05]],
        [['Магнат', '👑', 'cps', 1], ['Король рынка', '🤑', 'cost', 0.03], ['Золотые руки', '🪙', 'all', 0.1], ['Империя торговли', '🏛️', 'evReward', 0.3]],
      ] },
    { id: 'hunter', name: 'Охотник', icon: '🏹', role: 'боссы', desc: 'Урон по боссам ×1.5, души +25%',
      base: (a) => { a.mul('bossDmg', 1.5); a.mul('souls', 1.25); },
      rows: [
        [['Выслеживание', '🐾', 'bossDmg', 0.1], ['Ловкость', '🤸', 'critCh', 0.003], ['Трофеи', '🦴', 'souls', 0.05], ['Терпение', '⏱️', 'bossTime', 1]],
        [['Меткий выстрел', '🎯', 'bossDmg', 0.15], ['Сбор душ', '👻', 'souls', 0.08], ['Ловушки', '🪤', 'bossHp', 0.02], ['Снаряжение', '🎒', 'drop', 0.05]],
        [['Охота на крупного', '🐗', 'bossDmg', 0.25], ['Ярость охоты', '😤', 'critMul', 0.3], ['Следопыт', '🧭', 'evFreq', 0.05], ['Стая', '🐺', 'pet', 0.08]],
        [['Убийца титанов', '🗡️', 'bossDmg', 0.4], ['Жнец душ', '💀', 'souls', 0.15], ['Засада', '🌲', 'bossTime', 3], ['Боевая сила', '💪', 'combat', 0.15]],
        [['Легендарный охотник', '🏆', 'bossDmg', 1], ['Коллекционер', '🧳', 'souls', 0.3], ['Хищник', '🐯', 'combat', 0.3], ['Великая охота', '🌕', 'all', 0.1]],
      ] },
    { id: 'alch', name: 'Алхимик', icon: '⚗️', role: 'бустеры', desc: 'Бустеры: длительность и сила ×1.25, зелья ×1.5',
      base: (a) => { a.mul('boostDur', 1.25); a.mul('boostPow', 1.25); a.mul('potion', 1.5); },
      rows: [
        [['Эликсир', '🧪', 'boostDur', 0.05], ['Катализатор', '⚗️', 'boostPow', 0.05], ['Травник', '🌿', 'farm', 0.1], ['Перегонка', '🔁', 'boostCd', 0.02]],
        [['Двойная доза', '💊', 'boostPow', 0.08], ['Стабилизатор', '🧊', 'boostDur', 0.08], ['Зельевар', '🍶', 'potion', 0.1], ['Удобрения', '🌱', 'farmSpeed', 0.1]],
        [['Философский камень', '🪨', 'boostPow', 0.12], ['Трансмутация', '✨', 'evReward', 0.15], ['Быстрая варка', '⏩', 'boostCd', 0.03], ['Мутаген', '🧬', 'pet', 0.1]],
        [['Великое делание', '🔥', 'boostDur', 0.15], ['Эссенция', '💧', 'potion', 0.2], ['Золото из свинца', '🪙', 'all', 0.05], ['Сад алхимика', '🌺', 'farm', 0.2]],
        [['Магистр', '🧙‍♂️', 'boostPow', 0.3], ['Вечный эликсир', '♾️', 'boostDur', 0.3], ['Панацея', '💖', 'potion', 0.5], ['Абсолют', '🌟', 'all', 0.1]],
      ] },
  ];
  const BYC = {};
  CLASSES.forEach((c) => (BYC[c.id] = c));
  const NODE_MAX = 5, ROW_REQ = 5;
  let selNode = null;

  const xpNeed = (lv) => Math.round(50 * Math.pow(lv, 1.7) + 50);

  const M = G.mod({
    id: 'hero',
    reset: 3,
    STATS, CLASSES,
    state: () => ({ lv: 1, xp: 0, pts: 0, st: { str: 0, agi: 0, luk: 0, int: 0, end: 0 }, cls: '', trees: {}, clsAt: 0, tab: 'stats' }),
    xpNeed,
    // начисление опыта
    gainXp(v) {
      const s = G.S.hero;
      if (!G.has('hero')) return;
      s.xp += v * G.st.xp;
      let ups = 0;
      while (s.xp >= xpNeed(s.lv) && ups < 1000) {
        s.xp -= xpNeed(s.lv);
        s.lv++;
        s.pts += 5;
        ups++;
      }
      if (ups) {
        G.toast(`⭐ Уровень героя ${s.lv}! +${5 * ups} очков характеристик`, 'ach');
        SFX.play('level');
        G.emit('heroLevel', s.lv);
        G.dirty = true;
      }
    },
    spent(cid) { const t = G.S.hero.trees[cid] || {}; let n = 0; for (const k in t) n += t[k]; return n; },
    clsPts(cid) { return Math.max(0, G.S.hero.lv - 1 - this.spent(cid)); },
    // боевые параметры (подземелья, башня)
    power() { const s = G.S.hero; return (10 + 4 * s.lv + 2 * s.st.str + s.st.agi + s.st.end) * G.st.combat; },
    hp() { const s = G.S.hero; return (100 + 20 * s.lv + 15 * s.st.end) * G.st.combatHp; },
    stats(acc) {
      const s = G.S.hero;
      if (!G.has('hero')) return;
      acc.mul('all', 1 + 0.03 * (s.lv - 1));        // +3% ко всему за уровень
      for (const k in STATS) if (s.st[k]) STATS[k][3](acc, s.st[k]);
      const c = BYC[s.cls];
      if (c) {
        c.base(acc);
        const t = s.trees[c.id] || {};
        c.rows.forEach((row, r) => row.forEach((n, i) => { const l = t[r + '_' + i]; if (l) G.applyStat(acc, n[2], n[3] * l); }));
      }
    },
    levels() { const s = G.S.hero; let n = s.lv; for (const c of CLASSES) n += this.spent(c.id); return n; },

    panels: [
      {
        id: 'hero', group: 'hero', name: 'Герой', icon: '🦸', order: 1,
        unlock: () => (G.S.stats.bossWins || 0) >= 1,
        hint: 'Победите первого босса',
        render: renderHero,
        badge: () => G.S.hero.pts > 0,
      },
      {
        id: 'class', group: 'hero', name: 'Класс', icon: '⚔️', order: 2,
        unlock: () => G.S.hero.lv >= 5,
        hint: 'Уровень героя 5',
        render: renderClass,
        badge: () => !G.S.hero.cls || M.clsPts(G.S.hero.cls) > 0,
      },
    ],
    actions: {
      stat(k, n) {
        const s = G.S.hero;
        n = n === 'all' ? s.pts : Math.min(+n, s.pts);
        if (n <= 0) return;
        s.st[k] += n; s.pts -= n;
        SFX.play('buy');
      },
      respec() {
        const s = G.S.hero, cost = 5 * s.lv;
        UI.confirm('🔄 Сброс характеристик', `Вернуть все очки за ${cost} 👻?`, () => {
          if (!G.pay({ souls: cost })) { G.toast('Не хватает душ', 'bad'); return; }
          for (const k in s.st) { s.pts += s.st[k]; s.st[k] = 0; }
        }, 'Сбросить');
      },
      cls(id) {
        const s = G.S.hero;
        if (!BYC[id] || s.cls === id) return;
        const left = (s.clsAt + 600000 - Date.now()) / 1000;
        if (s.cls && left > 0) { G.toast(`⏱ Сменить класс можно через ${U.time(left)}`); return; }
        s.cls = id; s.clsAt = Date.now();
        SFX.play('level');
        G.toast(`${BYC[id].icon} Класс: ${BYC[id].name}`, 'good');
      },
      node(key) { selNode = key; },
      learn(key) {
        const s = G.S.hero, c = BYC[s.cls];
        if (!c) return;
        const [r] = key.split('_').map(Number);
        const t = (s.trees[c.id] = s.trees[c.id] || {});
        if ((t[key] || 0) >= NODE_MAX || M.clsPts(c.id) < 1 || M.spent(c.id) < r * ROW_REQ) { SFX.play('error'); return; }
        t[key] = (t[key] || 0) + 1;
        SFX.play('buy');
      },
      resetTree() {
        const s = G.S.hero;
        if (!s.cls) return;
        UI.confirm('🔄 Сброс дерева', `Вернуть очки класса «${BYC[s.cls].name}» за 20 👻?`, () => {
          if (!G.pay({ souls: 20 })) { G.toast('Не хватает душ', 'bad'); return; }
          s.trees[s.cls] = {};
        }, 'Сбросить');
      },
    },
  });
  G.defStat('combat', 1);
  G.defStat('combatHp', 1);
  G.defStat('research', 1);
  G.defStat('potion', 1);

  /* ---------- Источники опыта ---------- */
  G.on('tap', (ctx) => { if (!ctx.auto) M.gainXp(1); });
  G.on('buy', (kind, n) => M.gainXp(2 * (n || 1)));
  G.on('bossWin', (n) => M.gainXp(100 * (n / 50)));
  G.on('golden', () => M.gainXp(25));
  G.on('chest', () => M.gainXp(60));
  G.on('ach', () => M.gainXp(50));
  G.on('prestige', (l) => M.gainXp(500 * l));
  G.on('skill', () => M.gainXp(10));

  /* ---------- Панели ---------- */
  function renderHero() {
    const s = G.S.hero;
    const need = xpNeed(s.lv);
    let h = `<div class="pcard"><div class="row"><span style="font-size:40px">🦸</span><div style="flex:1">
      <div class="ctitle">Герой · уровень ${s.lv}</div>${UI.bar(s.xp / need, 'gold', `${U.fint(s.xp)} / ${U.fint(need)} опыта`)}</div></div>
      <div class="row dim small"><span>⚔️ Сила ${U.fmt(M.power())} · ❤️ ${U.fmt(M.hp())}</span><span>+${(s.lv - 1) * 3}% ко всему доходу</span></div></div>`;
    h += UI.sec('Характеристики', `свободно очков: <b class="hl">${s.pts}</b>`);
    for (const k in STATS) {
      const [name, icon, desc] = STATS[k];
      const ok = s.pts > 0;
      h += UI.card({
        k: 'hs' + k, icon, title: name, lv: String(s.st[k]), desc,
        btn: `<div class="btnrow tight"><button class="mini${ok ? ' ok' : ''}" data-a="hero.stat:${k}:1"${U.dis(ok)}>+1</button><button class="mini${ok ? ' ok' : ''}" data-a="hero.stat:${k}:5"${U.dis(ok)}>+5</button><button class="mini${ok ? ' ok' : ''}" data-a="hero.stat:${k}:all"${U.dis(ok)}>Все</button></div>`,
      });
    }
    h += `<button class="btn wide" data-a="hero.respec">🔄 Сбросить характеристики (${5 * s.lv} 👻)</button>`;
    h += UI.info('Опыт: тапы, покупки, боссы, события, достижения, навыки и престиж.', 'dim');
    return h;
  }

  function renderClass() {
    const s = G.S.hero;
    let h = '<div class="classes">';
    for (const c of CLASSES) {
      h += `<button class="clsb${s.cls === c.id ? ' on' : ''}" data-a="hero.cls:${c.id}" data-k="${c.id}"><span>${c.icon}</span><b>${c.name}</b><small>${c.role}</small></button>`;
    }
    h += '</div>';
    const c = BYC[s.cls];
    if (!c) return h + UI.info('Выберите класс. Каждый класс усиливает свой стиль игры и имеет дерево из 20 узлов. Сменить класс можно раз в 10 минут, вложенные очки сохраняются.');
    const t = s.trees[c.id] || {}, sp = M.spent(c.id), pts = M.clsPts(c.id);
    h += UI.info(`${c.icon} <b>${c.name}</b>: ${c.desc}<br>Очков класса: <b class="hl">${pts}</b> · вложено ${sp} (1 очко за уровень героя)`);
    if (selNode) {
      const [r, i] = selNode.split('_').map(Number);
      const n = c.rows[r] && c.rows[r][i];
      if (n) {
        const l = t[selNode] || 0, locked = sp < r * ROW_REQ;
        const ok = !locked && l < NODE_MAX && pts > 0;
        h += `<div class="tsel" data-k="hsel" style="--bc:#ffd34d"><div class="cico">${n[1]}</div><div class="cmain"><div class="ctitle">${n[0]} <span class="lv">${l}/${NODE_MAX}</span></div>
          <div class="cdesc">${l ? G.statText(n[2], n[3] * l) : 'Не изучен'}${l < NODE_MAX ? ` → <span class="hl">${G.statText(n[2], n[3] * (l + 1))}</span>` : ''}</div>
          ${locked ? `<div class="cdesc bad">Нужно вложить ${r * ROW_REQ} очков в дерево</div>` : ''}</div>
          <div class="cbtn"><button class="buy${ok ? ' ok' : ''}" data-a="hero.learn:${selNode}"${U.dis(ok)}><small>Изучить</small><span>1 очко</span></button></div></div>`;
      }
    }
    h += '<div class="ctree">';
    c.rows.forEach((row, r) => {
      const locked = sp < r * ROW_REQ;
      h += `<div class="crow${locked ? ' lock' : ''}"><span class="creq">${r * ROW_REQ}</span>`;
      row.forEach((n, i) => {
        const key = r + '_' + i, l = t[key] || 0;
        h += `<button class="tnode${l ? ' has' : ''}${l >= NODE_MAX ? ' max' : ''}${selNode === key ? ' sel' : ''}${locked ? ' lock' : ''}" style="--bc:#ffd34d" data-a="hero.node:${key}" data-k="${key}"><span>${locked ? '🔒' : n[1]}</span><small>${l}/${NODE_MAX}</small></button>`;
      });
      h += '</div>';
    });
    h += '</div>';
    h += `<button class="btn wide" data-a="hero.resetTree">🔄 Сбросить дерево (20 👻)</button>`;
    return h;
  }

  G.statRow(30, () => [['🦸 Уровень героя', String(G.S.hero.lv)], ['⚔️ Боевая сила', U.fmt(M.power())]]);
  G.achCat('hero', 'Герой', '🦸', 'Опыт ×1.5', (a) => a.mul('xp', 1.5));
  G.achTier('hero', 'hl', ['Ученик', 'Подмастерье', 'Мастер', 'Чемпион', 'Герой', 'Полубог'], () => G.S.hero.lv, [5, 10, 25, 50, 75, 100], (t) => `Уровень героя ${t}`);
  G.achAdd('hero', 'hcls', 'Призвание', 'Выбрать класс', () => !!G.S.hero.cls);
  G.titleAdd('hero', 'Герой', '🦸', () => G.S.hero.lv >= 25, 'Боевая сила +25%, весь доход +10%', (a) => { a.mul('combat', 1.25); a.mul('all', 1.1); });
})();
