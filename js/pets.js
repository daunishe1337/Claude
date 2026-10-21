'use strict';
/* ============================================================
   pets.js — питомцы и экспедиции.
   • 15 питомцев 5 редкостей вылупляются из яиц;
   • пассивные бонусы, уровни до 50 (за корм), звёзды за
     дубликаты (до 5★), 3 стадии эволюции, навык со 2-й стадии;
   • слияние: 5 осколков питомцев одной редкости → яйцо выше;
   • экспедиции: питомцы уходят в походы по таймеру и приносят
     корм, яйца, души и предметы.
   ============================================================ */
(() => {
  G.defCur('eggs', 'Яйца', '🥚', 3, true);
  G.defCur('food', 'Корм', '🍖', 3);

  const RAR = [
    { name: 'Обычный', c: '#b8b8c8', w: 55, k: 1 },
    { name: 'Редкий', c: '#3aa0ff', w: 28, k: 1.6 },
    { name: 'Эпический', c: '#b45cff', w: 12, k: 2.6 },
    { name: 'Легендарный', c: '#ffb020', w: 4.2, k: 4 },
    { name: 'Мифический', c: '#ff3d5a', w: 0.8, k: 6.5 },
  ];
  // [id, редкость, имена 3 стадий, иконки 3 стадий, бонус [стат, база], навык [название, стат, значение]]
  const PETS = [
    ['cat', 0, ['Котёнок', 'Кошка', 'Лев'], ['🐱', '🐈', '🦁'], ['tap', 0.15], ['Острые когти', 'critMul', 0.5]],
    ['dog', 0, ['Щенок', 'Пёс', 'Волк'], ['🐶', '🐕', '🐺'], ['cps', 0.15], ['Верный страж', 'offline', 0.2]],
    ['ham', 0, ['Хомяк', 'Кролик', 'Кенгуру'], ['🐹', '🐰', '🦘'], ['offline', 0.2], ['Запасливость', 'evReward', 0.15]],
    ['chick', 0, ['Цыплёнок', 'Петух', 'Павлин'], ['🐥', '🐓', '🦚'], ['evReward', 0.15], ['Золотое яйцо', 'eggs', 0.1]],
    ['frog', 0, ['Лягушонок', 'Ящерица', 'Крокодил'], ['🐸', '🦎', '🐊'], ['xp', 0.2], ['Мудрость болот', 'research', 0.1]],
    ['fox', 1, ['Лис', 'Чернобурка', 'Кицунэ'], ['🦊', '🦊', '🦊'], ['critCh', 0.01], ['Хитрость', 'megaCh', 0.01]],
    ['owl', 1, ['Сова', 'Филин', 'Орёл'], ['🦉', '🦉', '🦅'], ['skillPow', 0.2], ['Ночное зрение', 'manaRegen', 0.2]],
    ['panda', 1, ['Панда', 'Медведь', 'Великий медведь'], ['🐼', '🐻', '🐻'], ['all', 0.1], ['Сила природы', 'combat', 0.2]],
    ['peng', 1, ['Пингвин', 'Тюлень', 'Морж'], ['🐧', '🦭', '🦭'], ['boostDur', 0.15], ['Холодная голова', 'boostCd', 0.05]],
    ['uni', 2, ['Единорог', 'Пегас', 'Аликорн'], ['🦄', '🦄', '🦄'], ['crystal', 0.1], ['Радуга', 'all', 0.1]],
    ['tiger', 2, ['Тигрёнок', 'Тигр', 'Саблезуб'], ['🐯', '🐅', '🐅'], ['bossDmg', 0.3], ['Рык', 'bossTime', 3]],
    ['octo', 2, ['Осьминог', 'Кальмар', 'Кракен'], ['🐙', '🦑', '🦑'], ['autoPow', 0.3], ['Восемь рук', 'fingers', 2]],
    ['dragon', 3, ['Дракончик', 'Дракон', 'Древний дракон'], ['🐲', '🐉', '🐉'], ['all', 0.3], ['Драконье пламя', 'tap', 0.5]],
    ['bird', 3, ['Птенец', 'Попугай', 'Жар-птица'], ['🐦', '🦜', '🦚'], ['souls', 0.3], ['Перерождение', 'crystal', 0.15]],
    ['whale', 4, ['Китёнок', 'Кит', 'Космокит'], ['🐳', '🐋', '🐋'], ['all', 0.6], ['Песнь звёзд', 'dust', 0.2]],
  ].map(([id, r, names, icons, bonus, skill]) => ({ id, r, names, icons, bonus, skill }));
  const BY = {};
  PETS.forEach((p) => (BY[p.id] = p));
  const STAR_NEED = [1, 2, 4, 8, 16];
  const EVO = [{ lv: 20, eggs: 3, souls: 20 }, { lv: 40, eggs: 10, souls: 100 }];
  // маршруты экспедиций: [название, иконка, минут, награды]
  const ROUTES = [
    { id: 'walk', name: 'Прогулка', icon: '🌳', min: 5, food: 6, egg: 0.03, souls: 0, item: 0 },
    { id: 'forest', name: 'Лесная тропа', icon: '🌲', min: 30, food: 30, egg: 0.2, souls: 1, item: 0.05 },
    { id: 'mount', name: 'Горный перевал', icon: '⛰️', min: 120, food: 100, egg: 0.8, souls: 6, item: 0.25 },
    { id: 'cave', name: 'Тёмная пещера', icon: '🕳️', min: 480, food: 320, egg: 2.5, souls: 25, item: 0.7 },
  ];
  let selPet = null;

  const M = G.mod({
    id: 'pets',
    reset: 3,
    PETS, RAR,
    state: () => ({ own: {}, active: [], exp: [], expSlots: 2, tab: 'list', route: 'walk', hatched: 0 }),
    slots() { return 3 + (G.st.petSlots || 0); },
    pet(id) { return G.S.pets.own[id]; },
    // сила бонуса питомца
    power(id) {
      const p = this.pet(id), d = BY[id];
      if (!p) return 0;
      return (1 + 0.1 * (p.lv - 1)) * (1 + 0.25 * p.stars) * Math.pow(2, p.evo) * G.st.pet;
    },
    bonusVal(id) { return BY[id].bonus[1] * this.power(id); },
    onExp(id) { return G.S.pets.exp.some((e) => e.pet === id); },
    stats(acc) {
      const s = G.S.pets;
      let sumLv = 0;
      for (const id in s.own) sumLv += s.own[id].lv;
      // коллекция: +0.5% ко всему доходу за каждые 10 уровней всех питомцев
      acc.mul('all', 1 + 0.005 * sumLv);
      for (const id of s.active) {
        const d = BY[id], p = s.own[id];
        if (!d || !p) continue;
        G.applyStat(acc, d.bonus[0], this.bonusVal(id));
        if (p.evo >= 1) G.applyStat(acc, d.skill[1], d.skill[2] * (1 + 0.1 * p.stars));
      }
    },
    levels() { const s = G.S.pets; let n = 0; for (const id in s.own) n += s.own[id].lv + s.own[id].stars + s.own[id].evo; return n; },
    lvCost(id) { const p = this.pet(id); return Math.round(8 * Math.pow(p.lv, 1.5) * RAR[BY[id].r].k); },

    // вылупление: возвращает id питомца
    hatch(minR = 0) {
      const s = G.S.pets;
      const luck = G.st.drop || 1;
      const r = Math.max(minR, U.wpick([0, 1, 2, 3, 4], (i) => RAR[i].w * (i >= 2 ? luck : 1)));
      const pool = PETS.filter((p) => p.r === r);
      const d = U.pick(pool);
      const own = s.own[d.id];
      s.hatched++;
      G.count('hatched');
      if (own) { own.shards++; return { d, dupe: true }; }
      s.own[d.id] = { lv: 1, stars: 0, evo: 0, shards: 0 };
      if (s.active.length < this.slots()) s.active.push(d.id);
      return { d, dupe: false };
    },
    tick() {
      // завершённые экспедиции
      const s = G.S.pets, now = Date.now();
      for (const e of s.exp) if (!e.done && now >= e.end) { e.done = true; G.toast(`🧭 Экспедиция «${ROUTES.find((r) => r.id === e.route).name}» завершена!`, 'good'); }
    },

    panels: [
      {
        id: 'pets', group: 'hero', name: 'Питомцы', icon: '🐾', order: 4,
        unlock: () => (G.S.stats.prestiges || 0) >= 1 || G.cur('eggs') > 0 || Object.keys(G.S.pets.own).length > 0,
        hint: 'Первый престиж или первое яйцо',
        render,
        badge: () => G.cur('eggs') > 0 || Object.keys(G.S.pets.own).length > 0 && G.S.pets.active.length < Math.min(M.slots(), Object.keys(G.S.pets.own).length),
      },
      {
        id: 'expeditions', group: 'hero', name: 'Экспедиции', icon: '🧭', order: 5,
        unlock: () => Object.keys(G.S.pets.own).length >= 3,
        hint: '3 питомца',
        render: renderExp,
        badge: () => G.S.pets.exp.some((e) => Date.now() >= e.end) || (G.S.pets.exp.length < G.S.pets.expSlots && freePets().length > 0),
      },
    ],
    actions: {
      tab(t) { G.S.pets.tab = t; },
      hatch(n) {
        n = n === 'all' ? Math.min(100, Math.floor(G.cur('eggs'))) : +n;
        if (n < 1 || G.cur('eggs') < n) { SFX.play('error'); return; }
        G.add('eggs', -n);
        const res = [];
        for (let i = 0; i < n; i++) res.push(M.hatch());
        SFX.play('open');
        FX.flash('rgba(255,211,77,.3)');
        const html = res.map(({ d, dupe }) => `<span class="pchip" style="--rc:${RAR[d.r].c}">${d.icons[0]} ${d.names[0]}${dupe ? ' <small>+осколок</small>' : ' <b>НОВЫЙ!</b>'}</span>`).join('');
        UI.modal({ title: '🥚 Из яйца вылупились:', html: `<div class="pchips">${html}</div>`, buttons: [{ text: 'Ура!', cls: 'pri' }] });
      },
      sel(id) { selPet = id; },
      toggle(id) {
        const s = G.S.pets;
        if (!s.own[id] || M.onExp(id)) return;
        const i = s.active.indexOf(id);
        if (i >= 0) s.active.splice(i, 1);
        else if (s.active.length < M.slots()) s.active.push(id);
        else { G.toast('Все слоты заняты — уберите питомца'); return; }
        SFX.play('buy');
      },
      feed(id, n) {
        const p = M.pet(id);
        if (!p) return;
        let k = 0;
        const max = n === 'max' ? 50 : +n;
        while (k < max && p.lv < 50) {
          const c = M.lvCost(id);
          if (G.cur('food') < c) break;
          G.add('food', -c); p.lv++; k++;
        }
        if (k) SFX.play('buy'); else SFX.play('error');
      },
      star(id) {
        const p = M.pet(id);
        if (!p || p.stars >= 5 || p.shards < STAR_NEED[p.stars]) return;
        p.shards -= STAR_NEED[p.stars]; p.stars++;
        SFX.play('level');
        G.toast(`⭐ ${BY[id].names[p.evo]}: ${p.stars}★`, 'good');
      },
      evo(id) {
        const p = M.pet(id), e = EVO[p ? p.evo : 9];
        if (!p || !e || p.lv < e.lv) return;
        const k = RAR[BY[id].r].k;
        const cost = { eggs: e.eggs, souls: Math.round(e.souls * k) };
        if (!G.pay(cost)) { SFX.play('error'); G.toast('Не хватает ресурсов', 'bad'); return; }
        p.evo++;
        SFX.play('win');
        FX.flash('rgba(163,104,255,.45)');
        G.toast(`🌟 Эволюция: ${BY[id].icons[p.evo]} ${BY[id].names[p.evo]}!`, 'ach');
        G.count('evolutions');
      },
      merge(r) {
        // 5 осколков питомцев редкости r → питомец редкости r+1
        r = +r;
        const s = G.S.pets;
        const list = PETS.filter((p) => p.r === r && s.own[p.id]).sort((a, b) => s.own[b.id].shards - s.own[a.id].shards);
        const total = list.reduce((a, p) => a + s.own[p.id].shards, 0);
        if (r >= 4 || total < 5) return;
        let need = 5;
        for (const p of list) { const t = Math.min(need, s.own[p.id].shards); s.own[p.id].shards -= t; need -= t; if (!need) break; }
        const { d, dupe } = M.hatch(r + 1);
        G.count('merges');
        SFX.play('win');
        G.toast(`🔀 Слияние: ${d.icons[0]} ${d.names[0]}${dupe ? ' (+осколок)' : ' — новый!'}`, 'ach');
      },
      route(id) { G.S.pets.route = id; },
      send(id) {
        const s = G.S.pets, r = ROUTES.find((x) => x.id === s.route);
        if (!s.own[id] || M.onExp(id) || s.exp.length >= s.expSlots) return;
        const i = s.active.indexOf(id);
        if (i >= 0) s.active.splice(i, 1);
        s.exp.push({ pet: id, route: r.id, end: Date.now() + r.min * 60000 * (G.st.expTime || 1) });
        SFX.play('open');
      },
      claim(i) {
        const s = G.S.pets, e = s.exp[i];
        if (!e || Date.now() < e.end) return;
        const r = ROUTES.find((x) => x.id === e.route);
        const pw = Math.sqrt(M.power(e.pet)) * RAR[BY[e.pet].r].k;
        const got = [];
        const food = Math.round(r.food * pw * U.rand(0.8, 1.2));
        G.add('food', food); got.push(`🍖 ${food}`);
        let eggs = Math.floor(r.egg * G.st.eggs + Math.random());
        if (Math.random() < (r.egg * G.st.eggs) % 1) eggs++;
        if (eggs) { G.add('eggs', eggs); got.push(`🥚 ${eggs}`); }
        if (r.souls) { const so = Math.round(r.souls * pw * G.st.souls); G.add('souls', so); got.push(`👻 ${so}`); }
        if (r.item && Math.random() < r.item && G.byId.gear && G.has('gear')) { const it = G.byId.gear.make(1 + G.byId.stage.worlds(), r.min >= 480 ? 2 : 0); G.byId.gear.give(it, true); got.push(`${G.byId.gear.SLOTS[it.slot][1]} ${it.name}`); }
        const coins = G.income() * r.min * 6;
        G.earn(coins, 'event'); got.push(`🪙 ${U.fmt(coins)}`);
        s.exp.splice(i, 1);
        G.count('expeditions');
        SFX.play('coin');
        G.toast(`🧭 ${BY[e.pet].icons[M.pet(e.pet).evo]} вернулся: ${got.join(' · ')}`, 'good');
      },
      slot() {
        const s = G.S.pets, c = 50 * Math.pow(3, s.expSlots - 2);
        if (s.expSlots >= 5 || !G.pay({ souls: c })) { SFX.play('error'); return; }
        s.expSlots++;
        SFX.play('buy');
      },
    },
  });
  G.defStat('petSlots', 0);
  G.defStat('eggs', 1);
  G.defStat('expTime', 1);

  const freePets = () => Object.keys(G.S.pets.own).filter((id) => !M.onExp(id));

  /* ---------- Источники яиц и корма ---------- */
  G.on('bossWin', (n, first) => {
    if (first || Math.random() < 0.3 * G.st.eggs) { G.add('eggs', 1); G.toast('🥚 +1 яйцо с босса'); }
    if (G.has('pets')) G.add('food', Math.round(10 * n / 50));
  });
  G.on('chest', (loot) => {
    if (Math.random() < 0.2 * G.st.eggs * G.st.drop) { G.add('eggs', 1); loot.list.push('🥚 яйцо'); }
    if (G.has('pets')) { const f = U.ri(5, 20); G.add('food', f); loot.list.push(`🍖 ${f}`); }
  });
  G.on('golden', () => { if (Math.random() < 0.03 * G.st.eggs) { G.add('eggs', 1); G.toast('🥚 Золотой объект принёс яйцо!'); } });
  G.on('prestige', () => { G.add('eggs', 1 + Math.floor(G.S.prestige.p1 / 5)); });

  /* ---------- Панели ---------- */
  const petTile = (id) => {
    const d = BY[id], p = M.pet(id), s = G.S.pets;
    const act = s.active.includes(id), exp = M.onExp(id);
    return `<button class="ptile${selPet === id ? ' sel' : ''}${act ? ' act' : ''}" style="--rc:${RAR[d.r].c}" data-a="pets.sel:${id}" data-k="${id}">
      <span>${d.icons[p.evo]}</span><b>${p.lv}</b><small>${'★'.repeat(p.stars)}</small>${exp ? '<i>🧭</i>' : ''}${act ? '<i>✔</i>' : ''}</button>`;
  };

  function render() {
    const s = G.S.pets;
    let h = `<div class="pcard"><div class="row"><span>🥚 Яйца: <b>${U.fint(G.cur('eggs'))}</b></span><span>🍖 Корм: <b>${U.fint(G.cur('food'))}</b></span></div>
      <div class="btnrow"><button class="btn gold" data-a="pets.hatch:1"${U.dis(G.cur('eggs') >= 1)}>🥚 Вылупить</button><button class="btn gold" data-a="pets.hatch:10"${U.dis(G.cur('eggs') >= 10)}>×10</button><button class="btn gold" data-a="pets.hatch:all"${U.dis(G.cur('eggs') >= 1)}>Все</button></div>
      <div class="center dim small">Активно ${s.active.length}/${M.slots()} · коллекция +${U.pct(0.005 * Object.values(s.own).reduce((a, p) => a + p.lv, 0), 1)} ко всему доходу</div></div>`;
    const ids = Object.keys(s.own);
    if (!ids.length) return h + UI.info('Питомцев пока нет. Яйца дают боссы, сундуки, золотые объекты, престиж и экспедиции.');
    // выбранный питомец
    const id = selPet && s.own[selPet] ? selPet : null;
    if (id) h += renderPet(id);
    h += UI.sec('Коллекция', `${ids.length}/${PETS.length}`);
    h += '<div class="pgrid">';
    for (const d of PETS) h += s.own[d.id] ? petTile(d.id) : `<div class="ptile none" style="--rc:${RAR[d.r].c}" data-k="${d.id}"><span>❔</span><small>${RAR[d.r].name}</small></div>`;
    h += '</div>';
    // слияние
    h += UI.sec('Слияние', '5 осколков одной редкости → питомец редкостью выше');
    for (let r = 0; r < 4; r++) {
      const tot = PETS.filter((p) => p.r === r && s.own[p.id]).reduce((a, p) => a + s.own[p.id].shards, 0);
      h += `<div class="row merge" data-k="mg${r}"><span style="color:${RAR[r].c}">${RAR[r].name}: осколков ${tot}</span>
        <button class="mini${tot >= 5 ? ' ok' : ''}" data-a="pets.merge:${r}"${U.dis(tot >= 5)}>🔀 → <span style="color:${RAR[r + 1].c}">${RAR[r + 1].name}</span></button></div>`;
    }
    return h;
  }

  function renderPet(id) {
    const d = BY[id], p = M.pet(id), s = G.S.pets;
    const act = s.active.includes(id), exp = M.onExp(id);
    const c = M.lvCost(id), e = EVO[p.evo];
    let h = `<div class="idet" style="--rc:${RAR[d.r].c}" data-k="pd${id}"><div class="row"><span style="font-size:44px">${d.icons[p.evo]}</span><div style="flex:1">
      <b style="color:${RAR[d.r].c}">${d.names[p.evo]}</b> <small class="dim">${RAR[d.r].name} · стадия ${p.evo + 1}/3</small>
      <div>${'★'.repeat(p.stars)}${'☆'.repeat(5 - p.stars)} <small class="dim">осколки ${p.shards}${p.stars < 5 ? '/' + STAR_NEED[p.stars] : ''}</small></div></div></div>`;
    h += UI.bar((p.lv - 1) / 49, 'green thin', `Уровень ${p.lv}/50`);
    h += `<div class="istats"><div>${G.statText(d.bonus[0], M.bonusVal(id))}${act ? '' : ' <span class="dim">(когда активен)</span>'}</div>
      <div class="${p.evo >= 1 ? '' : 'dim'}">Навык «${d.skill[0]}»: ${G.statText(d.skill[1], d.skill[2] * (1 + 0.1 * p.stars))}${p.evo >= 1 ? '' : ' — со 2-й стадии'}</div></div>`;
    h += '<div class="btnrow">';
    h += exp ? '<button class="btn" disabled>🧭 В экспедиции</button>' : `<button class="btn ${act ? '' : 'mag'}" data-a="pets.toggle:${id}">${act ? 'Убрать из отряда' : 'В отряд'}</button>`;
    if (p.lv < 50) h += `<button class="btn pri" data-a="pets.feed:${id}:1"${U.dis(G.cur('food') >= c)}>🍖 ${U.fmt(c)}</button><button class="btn pri" data-a="pets.feed:${id}:max"${U.dis(G.cur('food') >= c)}>MAX</button>`;
    h += '</div><div class="btnrow">';
    if (p.stars < 5) h += `<button class="btn" data-a="pets.star:${id}"${U.dis(p.shards >= STAR_NEED[p.stars])}>⭐ Звезда (${STAR_NEED[p.stars]} оск.)</button>`;
    if (e) h += `<button class="btn gold" data-a="pets.evo:${id}"${U.dis(p.lv >= e.lv)}>🌟 Эволюция: ур.${e.lv} · 🥚${e.eggs} 👻${Math.round(e.souls * RAR[d.r].k)}</button>`;
    return h + '</div></div>';
  }

  function renderExp() {
    const s = G.S.pets, now = Date.now();
    let h = UI.info(`Экспедиций: <b>${s.exp.length}/${s.expSlots}</b>. Питомец в походе не даёт бонус. Награда зависит от силы питомца.`);
    s.exp.forEach((e, i) => {
      const r = ROUTES.find((x) => x.id === e.route), d = BY[e.pet], left = (e.end - now) / 1000;
      const total = r.min * 60 * (G.st.expTime || 1);
      h += UI.card({
        k: 'ex' + i, icon: d.icons[M.pet(e.pet).evo], title: `${r.icon} ${r.name}`, desc: d.names[M.pet(e.pet).evo],
        extra: UI.bar(1 - Math.max(0, left) / total, 'green thin', left > 0 ? U.time(left) : 'Готово!'),
        btn: left > 0 ? '' : `<button class="buy ok" data-a="pets.claim:${i}">Забрать</button>`, cls: left > 0 ? '' : 'can',
      });
    });
    if (s.expSlots < 5) h += `<button class="btn wide" data-a="pets.slot">➕ Слот экспедиции (${50 * Math.pow(3, s.expSlots - 2)} 👻)</button>`;
    h += UI.sec('Маршрут');
    h += '<div class="chips small">' + ROUTES.map((r) => `<button class="chip${s.route === r.id ? ' on' : ''}" data-a="pets.route:${r.id}">${r.icon} ${r.name} · ${U.time(r.min * 60)}</button>`).join('') + '</div>';
    const free = freePets();
    if (s.exp.length < s.expSlots) {
      h += UI.sec('Отправить питомца');
      h += '<div class="pgrid">' + free.map((id) => `<button class="ptile" style="--rc:${RAR[BY[id].r].c}" data-a="pets.send:${id}" data-k="s${id}"><span>${BY[id].icons[M.pet(id).evo]}</span><b>${M.pet(id).lv}</b></button>`).join('') + '</div>';
    }
    return h;
  }

  G.achCat('pets', 'Питомцы', '🐾', 'Бонусы питомцев +25%', (a) => a.mul('pet', 1.25));
  G.achTier('pets', 'pc', ['Друг животных', 'Зоопарк', 'Ковчег', 'Полная коллекция'], () => Object.keys(G.S.pets.own).length, [1, 5, 10, 15], (t) => `Собрать ${t} питомцев`);
  G.achTier('pets', 'pe', ['Метаморфоза', 'Эволюционист'], () => G.S.stats.evolutions || 0, [1, 10], (t) => `${t} эволюций`);
  G.achTier('pets', 'px', ['Путешественник', 'Экспедитор'], () => G.S.stats.expeditions || 0, [1, 50], (t) => `${t} экспедиций`);
  G.titleAdd('tamer', 'Укротитель', '🐾', () => Object.keys(G.S.pets.own).length >= 10, 'Бонусы питомцев +50%', (a) => a.mul('pet', 1.5));
})();
