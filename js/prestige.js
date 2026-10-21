'use strict';
/* ============================================================
   prestige.js — три уровня престижа.
   1) Престиж: сброс монет, зданий и прокачки забега → кристаллы 💎,
      дерево талантов (27 узлов, 3 ветки);
   2) Вознесение: сброс кристаллов и талантов → звёздная пыль ✨,
      мета-улучшения (автоматизация, сохранение прогресса…);
   3) Перерождение вселенной: сброс всего → осколки вселенной 🌌,
      законы вселенной и новые механики на каждом уровне вселенной.
   ============================================================ */
(() => {
  G.defCur('crystals', 'Кристаллы', '💎', 2, true);
  G.defCur('dust', 'Звёздная пыль', '✨', 3, true);
  G.defCur('shards', 'Осколки вселенной', '🌌', 9, true);

  /* ---------- Дерево талантов ---------- */
  // [id, название, иконка, макс. уровень, описание(L), эффект(acc, L)]
  const BR = [
    { id: 'A', name: 'Сила', color: '#ff3df2', nodes: [
      ['Сильные пальцы', '👊', 10, (l) => `Тап ×${1 + 0.5 * l}`, (a, l) => a.mul('tap', 1 + 0.5 * l)],
      ['Зоркий глаз', '🎯', 10, (l) => `+${l}% шанс крита`, (a, l) => a.add('critCh', 0.01 * l)],
      ['Разрушитель', '💥', 10, (l) => `+${0.5 * l} к силе крита`, (a, l) => a.add('critMul', 0.5 * l)],
      ['Комбо-гений', '🔥', 10, (l) => `+${10 * l} к длине комбо`, (a, l) => a.add('comboLen', 10 * l)],
      ['Ярость', '😤', 10, (l) => `Сила бустеров +${10 * l}%`, (a, l) => a.mul('boostPow', 1 + 0.1 * l)],
      ['Мегаудар', '☄️', 10, (l) => `+${2 * l}% шанс мегакрита`, (a, l) => a.add('megaCh', 0.02 * l)],
      ['Чутьё дохода', '💡', 10, (l) => `Тап +${l}% дохода/с`, (a, l) => a.add('tapCps', 0.01 * l)],
      ['Пальцы-призраки', '👻', 10, (l) => `+${l} бесплатных авто-пальцев`, (a, l) => a.add('fingers', l)],
      ['Титан тапа', '🗿', 5, (l) => `Тап ×${Math.pow(2, l)}`, (a, l) => a.mul('tap', Math.pow(2, l))],
    ] },
    { id: 'B', name: 'Богатство', color: '#ffd34d', nodes: [
      ['Инвестор', '📈', 10, (l) => `Доход зданий ×${1 + 0.5 * l}`, (a, l) => a.mul('cps', 1 + 0.5 * l)],
      ['Торгаш', '🏷️', 10, (l) => `Цены −${2 * l}%`, (a, l) => a.mul('cost', 1 - 0.02 * l)],
      ['Синергия', '🔗', 10, (l) => `Синергия зданий +${50 * l}%`, (a, l) => a.mul('synergy', 1 + 0.5 * l)],
      ['Стартовый капитал', '💰', 10, (l) => `Старт забега с ${U.fmt(l ? Math.pow(10, 2 + l) : 0)} 🪙`, () => {}],
      ['Мастер-строитель', '🏗️', 10, (l) => `Мастерство +${20 * l}%`, (a, l) => a.mul('masteryK', 1 + 0.2 * l)],
      ['Офлайн-магнат', '💤', 10, (l) => `Офлайн-доход ×${1 + 0.2 * l}`, (a, l) => a.mul('offline', 1 + 0.2 * l)],
      ['Сложный процент', '🏦', 20, (l) => `Весь доход +${5 * l}%`, (a, l) => a.mul('all', 1 + 0.05 * l)],
      ['Быстрый путь', '🛤️', 10, (l) => `Требования этапов −${5 * l}%`, (a, l) => a.mul('stageReq', Math.pow(0.95, l))],
      ['Империя', '👑', 5, (l) => `Доход зданий ×${Math.pow(2, l)}`, (a, l) => a.mul('cps', Math.pow(2, l))],
    ] },
    { id: 'C', name: 'Удача', color: '#22e6ff', nodes: [
      ['Везунчик', '🍀', 10, (l) => `События чаще на ${10 * l}%`, (a, l) => a.mul('evFreq', 1 + 0.1 * l)],
      ['Щедрость', '🎁', 10, (l) => `Награды событий +${20 * l}%`, (a, l) => a.mul('evReward', 1 + 0.2 * l)],
      ['Охотник', '🏹', 10, (l) => `Урон по боссам +${25 * l}%`, (a, l) => a.mul('bossDmg', 1 + 0.25 * l)],
      ['Выдержка', '⏱️', 10, (l) => `+${3 * l} с на бой с боссом`, (a, l) => a.add('bossTime', 3 * l)],
      ['Сборщик душ', '👻', 10, (l) => `Души +${20 * l}%`, (a, l) => a.mul('souls', 1 + 0.2 * l)],
      ['Мана-поток', '🔮', 10, (l) => `Мана: +${10 * l} макс., реген +${20 * l}%`, (a, l) => { a.add('manaMax', 10 * l); a.mul('manaRegen', 1 + 0.2 * l); }],
      ['Мастер бустеров', '🚀', 10, (l) => `Бустеры +${10 * l}% длит., −${3 * l}% перезарядки`, (a, l) => { a.mul('boostDur', 1 + 0.1 * l); a.mul('boostCd', Math.pow(0.97, l)); }],
      ['Резонанс', '💎', 10, (l) => `Кристаллы +${10 * l}%`, (a, l) => a.mul('crystal', 1 + 0.1 * l)],
      ['Фортуна', '🌈', 5, (l) => `Весь доход ×${Math.pow(1.5, l).toFixed(2)}`, (a, l) => a.mul('all', Math.pow(1.5, l))],
    ] },
  ];
  const TAL = {};
  BR.forEach((b) => b.nodes.forEach((n, i) => {
    const id = b.id + (i + 1);
    TAL[id] = { id, br: b, tier: i + 1, name: n[0], icon: n[1], max: n[2], desc: n[3], st: n[4] };
  }));
  const talCost = (t, l) => Math.max(1, Math.round(t * Math.pow(l + 1, 1.5) * G.st.talentCost));

  /* ---------- Мета-улучшения (звёздная пыль) ---------- */
  // [id, название, иконка, макс, базовая цена, рост, описание(L), эффект]
  const META = [
    ['autoB', 'Автостроитель', '🤖', 1, 3, 1, () => 'Авто-покупка зданий (переключатель)', null],
    ['autoT', 'Авто-тренер', '🦾', 1, 3, 1, () => 'Авто-покупка силы тапа (переключатель)', null],
    ['keepU', 'Память забега', '🧠', 1, 8, 1, () => 'Криты, комбо и авто-тапер не сбрасываются при престиже', null],
    ['keepB', 'Вечные бустеры', '🔋', 1, 6, 1, () => 'Прокачка бустеров, навыков и событий сохраняется', null],
    ['metaInc', 'Мета-доход', '♾️', 15, 1, 2, (l) => `Весь доход ×${Math.pow(2, l)}`, (a, l) => a.mul('all', Math.pow(2, l))],
    ['cryst', 'Кристальный дождь', '💎', 20, 2, 1.6, (l) => `Кристаллы ×${(1 + 0.25 * l).toFixed(2)}`, (a, l) => a.mul('crystal', 1 + 0.25 * l)],
    ['soulL', 'Связь душ', '👻', 10, 2, 1.7, (l) => `Души ×${1 + 0.5 * l}`, (a, l) => a.mul('souls', 1 + 0.5 * l)],
    ['fastSt', 'Короткий путь', '🛤️', 10, 2, 1.8, (l) => `Требования этапов ×${Math.pow(0.8, l).toFixed(2)}`, (a, l) => a.mul('stageReq', Math.pow(0.8, l))],
    ['startC', 'Звёздный старт', '🌠', 5, 3, 2.5, (l) => `Старт забега с ${U.fmt(l ? Math.pow(100, l + 2) : 0)} 🪙`, null],
    ['offH', 'Вечный сон', '🌙', 6, 2, 2, (l) => `+${2 * l} ч офлайн-лимита`, (a, l) => a.add('offlineH', 2 * l)],
    ['talC', 'Эхо талантов', '🌀', 5, 4, 2, (l) => `Таланты дешевле на ${10 * l}%`, (a, l) => a.mul('talentCost', 1 - 0.1 * l)],
    ['boostM', 'Буря бустеров', '⚡', 10, 2, 1.8, (l) => `Перезарядка бустеров ×${Math.pow(0.9, l).toFixed(2)}`, (a, l) => a.mul('boostCd', Math.pow(0.9, l))],
    ['autoP', 'Авто-престиж', '🔁', 1, 15, 1, () => 'Автоматический престиж при выбранной награде', null],
    ['dustM', 'Звёздная кузня', '✨', 10, 5, 2, (l) => `Звёздная пыль +${20 * l}%`, (a, l) => a.mul('dust', 1 + 0.2 * l)],
  ].map(([id, name, icon, max, base, r, desc, st]) => ({ id, name, icon, max, base, r, desc, st }));
  const metaCost = (m, l) => Math.round(m.base * Math.pow(m.r, l));

  /* ---------- Законы вселенной (осколки) ---------- */
  const LAWS = [
    ['lawAll', 'Закон изобилия', '🌌', 50, 1, 1.8, (l) => `Весь доход ×${Math.pow(3, l)}`, (a, l) => a.mul('all', Math.pow(3, l))],
    ['lawCr', 'Закон кристаллов', '💎', 20, 1, 1.8, (l) => `Кристаллы ×${Math.pow(2, l)}`, (a, l) => a.mul('crystal', Math.pow(2, l))],
    ['lawDust', 'Закон звёзд', '✨', 20, 2, 1.9, (l) => `Звёздная пыль ×${(1 + 0.5 * l).toFixed(1)}`, (a, l) => a.mul('dust', 1 + 0.5 * l)],
    ['lawTap', 'Закон касания', '👆', 30, 1, 1.7, (l) => `Тап ×${Math.pow(4, l)}`, (a, l) => a.mul('tap', Math.pow(4, l))],
    ['lawXp', 'Закон опыта', '📘', 20, 1, 1.8, (l) => `Опыт и души ×${(1 + 0.5 * l).toFixed(1)}`, (a, l) => { a.mul('xp', 1 + 0.5 * l); a.mul('souls', 1 + 0.5 * l); }],
    ['lawTime', 'Закон времени', '⏳', 10, 2, 2, (l) => `Бустеры и навыки перезаряжаются на ${5 * l}% быстрее`, (a, l) => { a.mul('boostCd', 1 - 0.05 * l); a.mul('skillCd', 1 - 0.05 * l); }],
  ].map(([id, name, icon, max, base, r, desc, st]) => ({ id, name, icon, max, base, r, desc, st }));
  // новые механики по номеру вселенной
  const UNI_FEATURES = [
    [1, '📜 Законы вселенной', 'Постоянные улучшения за осколки'],
    [2, '🕳️ Сингулярность', 'Доход растёт с временем в текущей вселенной'],
    [3, '🎖️ Двойной титул', 'Можно носить второй титул'],
    [4, '🌠 Звёздный ветер', 'Звёздная пыль ×2'],
    [5, '🌟 Вечная память', 'Таланты стоят на 25% дешевле'],
  ];

  let sel = 'A1';
  const S = () => G.S.prestige;

  const M = G.mod({
    id: 'prestige',
    reset: 0,
    TAL, META, LAWS, BR,
    state: () => ({
      p1: 0, p2: 0, p3: 0, uni: 0,
      crAsc: 0, crUni: 0, dustUni: 0,
      tal: {}, meta: {}, laws: {},
      autoB: false, autoT: false, autoP: false, autoPAt: 50,
      uniStart: Date.now(), bestRunT: 0, lastRunT: 0,
    }),
    onReset(layer) {
      const s = S();
      if (layer >= 2) { s.tal = {}; s.crAsc = 0; }
      if (layer >= 3) { s.meta = {}; s.crUni = 0; s.dustUni = 0; s.uniStart = Date.now(); s.autoB = s.autoT = s.autoP = false; }
    },
    talLv: (id) => S().tal[id] || 0,
    metaLv: (id) => S().meta[id] || 0,
    lawLv: (id) => S().laws[id] || 0,

    /* ----- формулы ----- */
    // Всего кристаллов за вознесение (по монетам за вознесение, L = log10).
    // Пока лестница зданий не пройдена, монеты растут как (множитель)^3.7 —
    // там нужен корень 10-й степени (1 💎 при 1e17, 10 при 1e27, 63 при 1e35),
    // дальше доход растёт линейно от множителя — кубический корень.
    crRaw(c) {
      const L = Math.log10(c + 1);
      if (L < 17) return 0;
      return L <= 35 ? Math.pow(10, (L - 17) / 10) : Math.pow(10, 1.8 + (L - 35) / 3);
    },
    crTotal() { return Math.floor(this.crRaw(G.S.c.asc) * G.st.crystal); },
    crGain() { return Math.max(0, this.crTotal() - S().crAsc); },
    // пыль: от кристаллов, заработанных за это вознесение (с 1000 💎)
    dustGain() { return S().crAsc < 1000 ? 0 : Math.floor(5 * Math.sqrt(S().crAsc / 1000) * G.st.dust); },
    shardGain() { return Math.floor(Math.sqrt(S().dustUni / 25) * (G.st.shardMult || 1)); },
    singularity() { return this.uni() >= 2 ? 1 + Math.log10(1 + (Date.now() - S().uniStart) / 60000) : 1; },
    uni: () => S().uni,

    stats(acc) {
      const s = S();
      // каждый кристалл, заработанный за вознесение: +10% к доходу
      acc.mul('all', 1 + 0.1 * s.crAsc);
      // каждая пыль за вселенную: +50% к доходу и +10% к кристаллам
      acc.mul('all', 1 + 0.5 * s.dustUni);
      acc.mul('crystal', 1 + 0.1 * s.dustUni);
      // каждая вселенная: ×10 ко всему
      if (s.uni) acc.mul('all', Math.pow(10, s.uni));
      for (const id in s.tal) if (s.tal[id] && TAL[id]) TAL[id].st(acc, s.tal[id]);
      for (const m of META) { const l = s.meta[m.id] || 0; if (l && m.st) m.st(acc, l); }
      for (const L of LAWS) { const l = s.laws[L.id] || 0; if (l) L.st(acc, l); }
      acc.mul('all', this.singularity());
      if (s.uni >= 4) acc.mul('dust', 2);
      if (s.uni >= 5) acc.mul('talentCost', 0.75);
    },
    levels() {
      const s = S();
      let n = 0;
      for (const k in s.tal) n += s.tal[k];
      for (const k in s.meta) n += s.meta[k];
      for (const k in s.laws) n += s.laws[k];
      return n;
    },
    farGoal() {
      const g = this.crGain();
      if (!g) return null;
      return { text: `🌟 Престиж: +${g} 💎`, eta: g >= 10 ? 'готово!' : 'рано', prio: g >= 10 ? 5 : 1 };
    },

    /* ----- автоматизация ----- */
    tick(dt) {
      const s = S();
      autoTimer += dt;
      if (autoTimer < 0.5) return; // дважды в секунду
      autoTimer = 0;
      if (s.autoB && this.metaLv('autoB')) autoBuy('buildings');
      if (s.autoT && this.metaLv('autoT')) autoBuy('tap');
      if (s.autoP && this.metaLv('autoP') && this.crGain() >= s.autoPAt) doP1(true);
    },

    panels: [
      {
        id: 'p1', group: 'pres', name: 'Престиж', icon: '💎', order: 1,
        unlock: () => S().p1 > 0 || M.crTotal() >= 1,
        hint: 'Заработайте 100aa монет за забег',
        render: renderP1,
        badge: () => M.crGain() >= Math.max(10, S().crAsc * 0.5) || Object.values(TAL).some((t) => canTal(t)),
      },
      {
        id: 'p2', group: 'pres', name: 'Вознесение', icon: '✨', order: 2,
        unlock: () => S().p2 > 0 || S().crAsc >= 250,
        hint: '250 кристаллов за вознесение',
        render: renderP2,
        badge: () => M.dustGain() >= 5 && S().p2 === 0,
      },
      {
        id: 'p3', group: 'pres', name: 'Вселенная', icon: '🌌', order: 3,
        unlock: () => S().p3 > 0 || S().dustUni >= 20,
        hint: '20 звёздной пыли за вселенную',
        render: renderP3,
      },
    ],

    actions: {
      sel(id) { sel = id; },
      tal(id) {
        const t = TAL[id];
        if (!canTal(t)) { SFX.play('error'); return; }
        G.S.cur.crystals -= talCost(t.tier, M.talLv(id));
        S().tal[id] = M.talLv(id) + 1;
        G.emit('buy', 'talent', 1);
        SFX.play('buy');
      },
      p1() {
        const g = M.crGain();
        if (g < 1) { SFX.play('error'); return; }
        UI.confirm('💎 Престиж', `<p>Вы получите <b class="hl">+${g} 💎</b> (сейчас ${S().crAsc} за вознесение).</p>
          <p class="dim">Сбросятся: монеты, здания, сила тапа, криты, комбо, авто-тапер, прокачка бустеров/навыков/событий, этапы и множитель боссов.</p>
          <p class="dim">Сохранятся: души, достижения, миры, офлайн-прокачка, кристаллы и таланты.</p>
          <p>Каждый кристалл: <b>+10%</b> ко всему доходу навсегда (до вознесения).</p>`, () => doP1(false), 'Престиж!');
      },
      p2() {
        const g = M.dustGain();
        if (g < 1) { SFX.play('error'); return; }
        UI.confirm('✨ Вознесение', `<p>Вы получите <b class="hl">+${g} ✨</b> звёздной пыли.</p>
          <p class="dim">Сбросятся: всё от престижа, а также кристаллы, таланты и офлайн-прокачка.</p>
          <p>Каждая пыль: <b>+50%</b> ко всему доходу и <b>+10%</b> к кристаллам. Мета-улучшения остаются.</p>`, () => doP2(), 'Вознестись!');
      },
      p3() {
        const g = M.shardGain();
        if (g < 1) { SFX.play('error'); return; }
        UI.confirm('🌌 Перерождение вселенной', `<p>Вы получите <b class="hl">+${g} 🌌</b> и вселенную №${S().uni + 2}.</p>
          <p class="dim">Сбросится ВСЁ: монеты, кристаллы, звёздная пыль, мета-улучшения, души, герой и все системы.</p>
          <p class="dim">Сохранятся: достижения, титулы, коллекции, осколки и законы вселенной.</p>
          <p>Каждая вселенная: <b>×10</b> ко всему доходу и новая механика.</p>`, () => doP3(), 'Переродить!');
      },
      meta(id) {
        const m = META.find((x) => x.id === id), l = M.metaLv(id);
        if (!m || l >= m.max || !G.pay({ dust: metaCost(m, l) })) { SFX.play('error'); return; }
        S().meta[id] = l + 1;
        SFX.play('buy');
        G.emit('buy', 'meta', 1);
      },
      law(id) {
        const L = LAWS.find((x) => x.id === id), l = M.lawLv(id);
        if (!L || l >= L.max || !G.pay({ shards: metaCost(L, l) })) { SFX.play('error'); return; }
        S().laws[id] = l + 1;
        SFX.play('buy');
      },
      toggle(k) { S()[k] = !S()[k]; },
      autoAt(d) { S().autoPAt = Math.max(5, Math.round(S().autoPAt * (d > 0 ? 1.5 : 1 / 1.5))); },
    },
  });
  G.defStat('talentCost', 1);
  G.defStat('dust', 1);
  let autoTimer = 0;

  function canTal(t) {
    const l = M.talLv(t.id);
    if (l >= t.max) return false;
    if (t.tier > 1 && M.talLv(t.br.id + (t.tier - 1)) < 1) return false;
    return G.cur('crystals') >= talCost(t.tier, l);
  }

  // авто-покупка: каждый раз самое дешёвое из зданий / линий тапа
  function autoBuy(kind) {
    const mod = G.byId[kind], pre = kind === 'tap' ? 't' : 'b', defs = kind === 'tap' ? mod.LINES : mod.B;
    for (let k = 0; k < 25; k++) {
      let best = -1, bc = Infinity;
      for (let i = 0; i < mod.shown(); i++) {
        const c = G.lvCost(pre + i, defs[i].cost, 1.15, G.S[kind].lv[i], 1);
        if (c < bc) { bc = c; best = i; }
      }
      if (best < 0 || G.cur('coins') < bc) break;
      G.S.cur.coins -= bc;
      G.S[kind].lv[best]++;
      G.dirty = true;
    }
  }

  /* ---------- Действия престижа ---------- */
  function startCoins() {
    const a = M.talLv('B4'), b = M.metaLv('startC');
    return (a ? Math.pow(10, 2 + a) : 0) + (b ? Math.pow(100, b + 2) : 0);
  }
  function celebrate(color, title) {
    SFX.play('prestige');
    FX.flash(color);
    FX.shake(true);
    G.toast(title, 'unlock');
    UI.actions.tab('tap');
  }
  function doP1(auto) {
    const s = S(), g = M.crGain();
    if (g < 1) return;
    const runT = (Date.now() - G.S.t.runStart) / 1000;
    // сохранение прокачки по мета-улучшениям
    const keep = {};
    if (M.metaLv('keepU')) ['crit', 'combo', 'autotap'].forEach((k) => (keep[k] = U.clone(G.S[k])));
    if (M.metaLv('keepB')) {
      keep.bu = U.clone(G.S.boosters.u); keep.sk = U.clone(G.S.skills.lv);
      keep.ev = { freq: G.S.events.freq, rew: G.S.events.rew, life: G.S.events.life };
    }
    G.add('crystals', g);
    s.crAsc += g; s.crUni += g;
    s.p1++;
    s.lastRunT = runT;
    if (!s.bestRunT || runT < s.bestRunT) s.bestRunT = runT;
    G.count('prestiges');
    G.resetLayer(1);
    for (const k of ['crit', 'combo', 'autotap']) if (keep[k]) G.S[k] = keep[k];
    if (keep.bu) { G.S.boosters.u = keep.bu; G.S.skills.lv = keep.sk; Object.assign(G.S.events, keep.ev); }
    G.add('coins', startCoins());
    G.recalc();
    G.emit('prestige', 1);
    if (!auto) celebrate('rgba(34,230,255,.6)', `💎 Престиж №${s.p1}: +${g} кристаллов!`);
    else G.toast(`🔁 Авто-престиж: +${g} 💎`, 'good');
  }
  function doP2() {
    const s = S(), g = M.dustGain();
    if (g < 1) return;
    G.add('dust', g);
    s.dustUni += g;
    s.p2++;
    G.count('ascensions');
    G.resetLayer(2);
    G.add('coins', startCoins());
    G.recalc();
    G.emit('prestige', 2);
    celebrate('rgba(255,61,242,.6)', `✨ Вознесение №${s.p2}: +${g} звёздной пыли!`);
  }
  function doP3() {
    const s = S(), g = M.shardGain();
    if (g < 1) return;
    G.add('shards', g);
    s.uni++;
    s.p3++;
    G.count('universes');
    G.resetLayer(3);
    G.recalc();
    G.emit('prestige', 3);
    celebrate('rgba(255,255,255,.8)', `🌌 Вселенная №${s.uni + 1}! +${g} осколков`);
  }

  /* ---------- Панели ---------- */
  function renderP1() {
    const s = S(), g = M.crGain();
    const runT = (Date.now() - G.S.t.runStart) / 1000;
    let h = `<div class="pcard p1"><div class="row"><span>💎 Кристаллы: <b>${U.fint(G.cur('crystals'))}</b></span><span class="dim">за вознесение ${U.fint(s.crAsc)}</span></div>
      <div class="big">+${U.fint(g)} 💎</div>
      <div class="center dim">Бонус кристаллов: ×${U.fmt(1 + 0.1 * s.crAsc)} → ×${U.fmt(1 + 0.1 * (s.crAsc + g))} к доходу</div>
      <div class="center dim">Забег: ${U.time(runT)} · престижей ${s.p1}${s.bestRunT ? ' · лучший ' + U.time(s.bestRunT) : ''}</div>
      <button class="btn mag wide" data-a="prestige.p1"${U.dis(g >= 1)}>🌟 Совершить престиж</button>
      ${g >= 1 && g < 10 && s.p1 < 2 ? '<div class="center dim small">Совет: выгоднее подождать +10 💎</div>' : ''}</div>`;
    if (M.metaLv('autoP')) {
      h += `<div class="card" data-k="autop"><div class="cmain"><div class="ctitle">🔁 Авто-престиж при +${s.autoPAt} 💎</div>
        <div class="minis"><button class="mini" data-a="prestige.autoAt:-1">−</button><button class="mini" data-a="prestige.autoAt:1">+</button></div></div>
        <div class="cbtn"><button class="switch${s.autoP ? ' on' : ''}" data-a="prestige.toggle:autoP"><i></i></button></div></div>`;
    }
    // дерево талантов
    const t = TAL[sel];
    const l = M.talLv(sel), max = l >= t.max;
    const locked = t.tier > 1 && M.talLv(t.br.id + (t.tier - 1)) < 1;
    const cost = talCost(t.tier, l);
    h += UI.sec('Дерево талантов', 'нажмите на узел');
    h += `<div class="tsel" data-k="tsel" style="--bc:${t.br.color}"><div class="cico">${t.icon}</div><div class="cmain">
      <div class="ctitle">${t.name} <span class="lv">${l}/${t.max}</span></div>
      <div class="cdesc">${l ? t.desc(l) : 'Не изучен'}${max ? '' : ` → <span class="hl">${t.desc(l + 1)}</span>`}</div>
      ${locked ? `<div class="cdesc bad">Нужен предыдущий узел ветки «${t.br.name}»</div>` : ''}</div>
      <div class="cbtn">${max ? '<button class="buy max" disabled>МАКС</button>' : `<button class="buy${canTal(t) ? ' ok' : ''}" data-a="prestige.tal:${sel}"${U.dis(canTal(t))}><small>Изучить</small><span>💎 ${cost}</span></button>`}</div></div>`;
    h += '<div class="tree">';
    for (const b of BR) {
      h += `<div class="tcol" style="--bc:${b.color}"><div class="thead">${b.name}</div>`;
      b.nodes.forEach((n, i) => {
        const id = b.id + (i + 1), tl = M.talLv(id), T = TAL[id];
        const lock = i > 0 && M.talLv(b.id + i) < 1;
        const cls = (sel === id ? ' sel' : '') + (lock ? ' lock' : tl >= T.max ? ' max' : canTal(T) ? ' ok' : '') + (tl ? ' has' : '');
        h += `${i ? '<i class="tlink"></i>' : ''}<button class="tnode${cls}" data-a="prestige.sel:${id}" data-k="${id}"><span>${lock ? '🔒' : T.icon}</span><small>${tl}/${T.max}</small></button>`;
      });
      h += '</div>';
    }
    return h + '</div>';
  }

  function renderP2() {
    const s = S(), g = M.dustGain();
    let h = `<div class="pcard p2"><div class="row"><span>✨ Пыль: <b>${U.fint(G.cur('dust'))}</b></span><span class="dim">за вселенную ${U.fint(s.dustUni)}</span></div>
      <div class="big">+${U.fint(g)} ✨</div>
      <div class="center dim">Источник — кристаллы за вознесение: ${U.fint(s.crAsc)} (нужно от 1000 💎: 1000 → 5 ✨, 4000 → 10 ✨)</div>
      <div class="center dim">Бонус пыли: ×${U.fmt(1 + 0.5 * s.dustUni)} к доходу, ×${U.fmt(1 + 0.1 * s.dustUni)} к кристаллам</div>
      <button class="btn mag wide" data-a="prestige.p2"${U.dis(g >= 1)}>✨ Вознестись</button></div>`;
    h += UI.sec('Мета-улучшения', 'за звёздную пыль, сохраняются до перерождения вселенной');
    for (const m of META) {
      const l = M.metaLv(m.id), max = l >= m.max, c = metaCost(m, l);
      let btn = max ? '<button class="buy max" disabled>Есть</button>' : UI.buyC('prestige.meta:' + m.id, { dust: c });
      if (max && (m.id === 'autoB' || m.id === 'autoT')) btn = `<button class="switch${s[m.id] ? ' on' : ''}" data-a="prestige.toggle:${m.id}"><i></i></button>`;
      h += UI.card({ k: 'm' + m.id, icon: m.icon, title: m.name, lv: m.max > 1 ? `${l}/${m.max}` : '', desc: m.desc(Math.max(l, m.max > 1 ? l + (max ? 0 : 1) : 1)), btn, cls: max ? 'done' : G.cur('dust') >= c ? 'can' : 'cant' });
    }
    return h;
  }

  function renderP3() {
    const s = S(), g = M.shardGain();
    let h = `<div class="pcard p3"><div class="row"><span>🌌 Вселенная №${s.uni + 1}</span><span>Осколки: <b>${U.fint(G.cur('shards'))}</b></span></div>
      <div class="big">+${U.fint(g)} 🌌</div>
      <div class="center dim">Источник — пыль за вселенную: ${U.fint(s.dustUni)} (25 ✨ → 1 🌌, 100 ✨ → 2 🌌)</div>
      <div class="center dim">Бонус вселенных: ×${U.fmt(Math.pow(10, s.uni))} ко всему доходу${s.uni >= 2 ? ` · сингулярность ×${M.singularity().toFixed(2)}` : ''}</div>
      <button class="btn gold wide" data-a="prestige.p3"${U.dis(g >= 1)}>🌌 Переродить вселенную</button></div>`;
    h += UI.sec('Механики вселенных');
    for (const [n, name, desc] of UNI_FEATURES) {
      h += UI.card({ k: 'uf' + n, icon: s.uni >= n ? '✅' : '🔒', title: name, desc: `${desc}<br><span class="dim">Вселенная №${n + 1}</span>`, cls: s.uni >= n ? 'done' : 'cant' });
    }
    if (s.uni >= 1) {
      h += UI.sec('Законы вселенной', 'за осколки, вечные');
      for (const L of LAWS) {
        const l = M.lawLv(L.id), max = l >= L.max, c = metaCost(L, l);
        h += UI.card({ k: 'l' + L.id, icon: L.icon, title: L.name, lv: `${l}/${L.max}`, desc: L.desc(max ? l : l + 1), btn: max ? '<button class="buy max" disabled>МАКС</button>' : UI.buyC('prestige.law:' + L.id, { shards: c }), cls: max ? 'done' : G.cur('shards') >= c ? 'can' : 'cant' });
      }
    }
    return h;
  }

  /* ---------- Достижения престижа ---------- */
  G.achCat('prestige', 'Престиж', '💎', 'Кристаллы ×1.5', (a) => a.mul('crystal', 1.5));
  G.achTier('prestige', 'pr', ['Перезапуск', 'Цикл', 'Колесо сансары', 'Вечное возвращение', 'Бесконечный цикл'], () => G.S.stats.prestiges || 0, [1, 5, 25, 100, 500], (t) => `Совершить ${t} престижей`);
  G.achTier('prestige', 'cr', ['Кристаллик', 'Друза', 'Кристальная пещера', 'Сердце кристалла'], () => G.S.life.crystals || 0, [10, 100, 1000, 1e4], (t) => `Получить ${U.fmt(t)} кристаллов`);
  G.achTier('prestige', 'as', ['Вознёсшийся', 'Звёздный странник', 'Владыка звёзд'], () => G.S.stats.ascensions || 0, [1, 5, 20], (t) => `Вознестись ${t} раз`);
  G.achTier('prestige', 'un', ['Творец', 'Демиург'], () => G.S.stats.universes || 0, [1, 3], (t) => `Переродить вселенную ${t} раз`);
  G.titleAdd('ascended', 'Вознёсшийся', '😇', () => (G.S.stats.ascensions || 0) >= 1, 'Кристаллы ×1.5', (a) => a.mul('crystal', 1.5));
  G.titleAdd('creator', 'Творец вселенных', '🌌', () => (G.S.stats.universes || 0) >= 1, 'Весь доход ×3', (a) => a.mul('all', 3));
  G.statRow(10, () => [
    ['💎 Престижей', U.fint(S().p1)], ['✨ Вознесений', U.fint(S().p2)], ['🌌 Вселенная', '№' + (S().uni + 1)],
    ['💎 Кристаллов за вознесение', U.fint(S().crAsc)], ['✨ Пыли за вселенную', U.fint(S().dustUni)],
  ]);
})();
