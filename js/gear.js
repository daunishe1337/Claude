'use strict';
/* ============================================================
   gear.js — экипировка, зачарование и руны.
   • Слоты: перчатки, кольцо, амулет, плащ; 6 редкостей,
     случайные характеристики; переплавка в обломки;
   • зачарование +1…+20: шанс неудачи, с +10 уровень может
     упасть — защищают свитки;
   • руны: 6 типов × 10 уровней, вставляются в гнёзда предметов,
     3 одинаковые объединяются в руну уровнем выше.
   Предметы выпадают с боссов, сундуков, событий и подземелий.
   ============================================================ */
(() => {
  G.defCur('scrap', 'Обломки', '⚙️', 3);
  G.defCur('scrolls', 'Защитные свитки', '📜', 3);

  const SLOTS = { gloves: ['Перчатки', '🧤'], ring: ['Кольцо', '💍'], amulet: ['Амулет', '📿'], cloak: ['Плащ', '🧥'] };
  const SLOT_IDS = Object.keys(SLOTS);
  const RAR = [
    { name: 'Обычный', c: '#b8b8c8', k: 1, n: 1, sock: 0, w: 50 },
    { name: 'Необычный', c: '#45ff9a', k: 1.5, n: 2, sock: 1, w: 28 },
    { name: 'Редкий', c: '#3aa0ff', k: 2.2, n: 2, sock: 1, w: 14 },
    { name: 'Эпический', c: '#b45cff', k: 3.2, n: 3, sock: 2, w: 6 },
    { name: 'Легендарный', c: '#ffb020', k: 4.5, n: 3, sock: 2, w: 1.7 },
    { name: 'Мифический', c: '#ff3d5a', k: 6.5, n: 4, sock: 3, w: 0.3 },
  ];
  const POOL = {
    gloves: [['tap', 0.10], ['critCh', 0.005], ['critMul', 0.2], ['autoPow', 0.10], ['megaCh', 0.004]],
    ring: [['all', 0.05], ['crystal', 0.03], ['souls', 0.05], ['critCh', 0.004], ['drop', 0.04]],
    amulet: [['cps', 0.08], ['skillPow', 0.08], ['manaMax', 5], ['xp', 0.08], ['combat', 0.06]],
    cloak: [['boostDur', 0.05], ['offline', 0.06], ['evReward', 0.08], ['bossDmg', 0.10], ['combatHp', 0.08]],
  };
  const SUFFIX = ['силы', 'удачи', 'бури', 'дракона', 'титана', 'мудреца', 'теней', 'звёзд', 'пламени', 'льда', 'короля', 'странника', 'феникса', 'бездны'];
  const RUNES = {
    str: ['Руна силы', '🔴', 'tap', 0.05],
    gold: ['Руна богатства', '🟡', 'cps', 0.05],
    luck: ['Руна удачи', '🟢', 'critCh', 0.003],
    fury: ['Руна ярости', '🟣', 'critMul', 0.15],
    time: ['Руна времени', '🔵', 'boostDur', 0.03],
    wis: ['Руна мудрости', '⚪', 'xp', 0.05],
  };
  const RUNE_IDS = Object.keys(RUNES);
  const runeVal = (t, l) => RUNES[t][3] * l * Math.pow(1.25, l - 1);
  const MAX_INV = 30, MAX_ENCH = 20;
  let sel = null, sockPick = -1;

  const M = G.mod({
    id: 'gear',
    reset: 3,
    SLOTS, RAR, RUNES,
    state: () => ({ items: {}, eq: { gloves: null, ring: null, amulet: null, cloak: null }, runes: {}, next: 1, useScroll: true, tab: 'eq', starter: false }),
    // создать предмет: tier — сила источника, minR — минимальная редкость
    make(tier = 1, minR = 0, slot) {
      const s = G.S.gear;
      const luck = G.st.drop || 1;
      const r = Math.max(minR, U.wpick(RAR.map((x, i) => i), (i) => RAR[i].w * (i >= 2 ? luck : 1)));
      slot = slot || U.pick(SLOT_IDS);
      const R = RAR[r];
      const pool = POOL[slot].slice();
      const stats = [];
      for (let i = 0; i < R.n && pool.length; i++) {
        const [k, b] = pool.splice(Math.floor(Math.random() * pool.length), 1)[0];
        stats.push([k, +(b * R.k * U.rand(0.8, 1.2) * (1 + 0.15 * tier)).toPrecision(3)]);
      }
      const it = { id: s.next++, slot, r, tier, name: `${SLOTS[slot][0]} ${U.pick(SUFFIX)}`, stats, ench: 0, sock: Array(R.sock).fill(null) };
      return it;
    },
    // выдать предмет игроку
    give(it, quiet) {
      const s = G.S.gear;
      if (this.invList().length >= MAX_INV) {
        const sc = scrapOf(it);
        G.add('scrap', sc);
        if (!quiet) G.toast(`🎒 Инвентарь полон: ${it.name} → +${sc} ⚙️`);
        return;
      }
      s.items[it.id] = it;
      G.count('items');
      if (!quiet) G.toast(`🎁 Предмет: <b style="color:${RAR[it.r].c}">${SLOTS[it.slot][1]} ${it.name}</b>`, 'good');
    },
    giveRune(t, l = 1, n = 1) { const k = t + '_' + l; G.S.gear.runes[k] = (G.S.gear.runes[k] || 0) + n; },
    invList() { const s = G.S.gear, eqIds = Object.values(s.eq); return Object.values(s.items).filter((it) => !eqIds.includes(it.id)); },
    mult(it) { return (1 + 0.1 * it.ench) * G.st.gearPow; },
    stats(acc) {
      const s = G.S.gear;
      for (const sl of SLOT_IDS) {
        const it = s.items[s.eq[sl]];
        if (!it) continue;
        const m = this.mult(it);
        for (const [k, v] of it.stats) G.applyStat(acc, k, v * m);
        for (const rn of it.sock) if (rn) { const [t, l] = rn.split('_'); G.applyStat(acc, RUNES[t][2], runeVal(t, +l)); }
      }
    },
    levels() {
      const s = G.S.gear;
      let n = 0;
      for (const sl of SLOT_IDS) { const it = s.items[s.eq[sl]]; if (it) n += it.ench + it.r; }
      return n;
    },
    panels: [{
      id: 'gear', group: 'hero', name: 'Экипировка', icon: '🧤', order: 3,
      unlock: () => G.S.hero.lv >= 8 || (G.S.stats.prestiges || 0) >= 1,
      hint: 'Уровень героя 8 или первый престиж',
      render,
      badge: () => SLOT_IDS.some((sl) => !G.S.gear.eq[sl] && M.invList().some((it) => it.slot === sl)),
    }],
    actions: {
      tab(t) { G.S.gear.tab = t; sockPick = -1; },
      sel(id) { sel = +id; sockPick = -1; },
      equip(id) {
        const s = G.S.gear, it = s.items[id];
        if (!it) return;
        s.eq[it.slot] = it.id;
        SFX.play('buy');
      },
      unequip(slot) { G.S.gear.eq[slot] = null; SFX.play('buy'); },
      ench(id) {
        const s = G.S.gear, it = s.items[id];
        if (!it || it.ench >= MAX_ENCH) return;
        const c = enchCost(it);
        if (!G.pay(c)) { SFX.play('error'); G.toast('Не хватает ресурсов', 'bad'); return; }
        G.count('enchants');
        if (Math.random() < enchChance(it.ench)) {
          it.ench++;
          SFX.play('level');
          G.toast(`✨ Успех! ${it.name} +${it.ench}`, 'good');
          G.emit('enchant', it.ench);
        } else if (it.ench >= 10) {
          if (s.useScroll && G.cur('scrolls') >= 1) { G.add('scrolls', -1); G.toast('📜 Неудача, но свиток защитил предмет', 'bad'); }
          else { it.ench--; G.toast(`💔 Неудача: ${it.name} → +${it.ench}`, 'bad'); }
          SFX.play('fail');
        } else { SFX.play('fail'); G.toast('💨 Неудача зачарования', 'bad'); }
      },
      scroll() { G.S.gear.useScroll = !G.S.gear.useScroll; },
      melt(id) {
        const s = G.S.gear, it = s.items[id];
        if (!it || Object.values(s.eq).includes(it.id)) return;
        meltItem(it);
        sel = null;
        SFX.play('coin');
      },
      meltLow() {
        const list = M.invList().filter((it) => it.r <= 1 && it.ench === 0);
        if (!list.length) return;
        let sc = 0;
        list.forEach((it) => { sc += meltItem(it, true); });
        G.toast(`♻️ Переплавлено ${list.length} предм. → +${sc} ⚙️`, 'good');
        SFX.play('coin');
      },
      sock(i) { sockPick = sockPick === +i ? -1 : +i; },
      insert(key) {
        const s = G.S.gear, it = s.items[sel];
        if (!it || sockPick < 0 || !s.runes[key]) return;
        if (it.sock[sockPick]) M.giveRune(...it.sock[sockPick].split('_').map((x, j) => (j ? +x : x)));
        it.sock[sockPick] = key;
        s.runes[key]--;
        if (!s.runes[key]) delete s.runes[key];
        sockPick = -1;
        SFX.play('buy');
      },
      unsock(i) {
        const s = G.S.gear, it = s.items[sel];
        if (!it || !it.sock[i]) return;
        const [t, l] = it.sock[i].split('_');
        M.giveRune(t, +l);
        it.sock[i] = null;
      },
      merge(key) {
        const s = G.S.gear, [t, l] = key.split('_');
        if ((s.runes[key] || 0) < 3 || +l >= 10) return;
        s.runes[key] -= 3;
        if (!s.runes[key]) delete s.runes[key];
        M.giveRune(t, +l + 1);
        G.count('runeMerges');
        SFX.play('level');
      },
      mergeAll() {
        const s = G.S.gear;
        let n = 0;
        for (let l = 1; l < 10; l++) for (const t of RUNE_IDS) {
          const k = t + '_' + l;
          while ((s.runes[k] || 0) >= 3) { s.runes[k] -= 3; M.giveRune(t, l + 1); n++; }
          if (s.runes[k] === 0) delete s.runes[k];
        }
        if (n) { G.count('runeMerges', n); SFX.play('level'); G.toast(`🔮 Объединено рун: ${n}`, 'good'); }
      },
    },
  });
  G.defStat('gearPow', 1);

  function enchChance(e) { return e < 5 ? 1 : e < 10 ? 0.9 - 0.06 * (e - 5) : Math.max(0.25, 0.6 - 0.035 * (e - 10)); }
  function enchCost(it) {
    const c = { scrap: Math.round(4 * Math.pow(it.ench + 1, 1.6) * (it.r + 1)) };
    if (it.ench >= 10) c.souls = (it.ench - 9) * 3 * (it.r + 1);
    return c;
  }
  function scrapOf(it) { return Math.round(Math.pow(it.r + 1, 2) * 3 * (1 + 0.5 * it.ench) * (1 + 0.1 * it.tier)); }
  function meltItem(it, quiet) {
    const s = G.S.gear, sc = scrapOf(it);
    G.add('scrap', sc);
    for (const rn of it.sock) if (rn) { const [t, l] = rn.split('_'); M.giveRune(t, +l); }
    if (Math.random() < 0.25) M.giveRune(U.pick(RUNE_IDS), 1);
    delete s.items[it.id];
    G.count('melted');
    if (!quiet) G.toast(`♻️ +${sc} ⚙️`);
    return sc;
  }

  /* ---------- Добыча ---------- */
  const on = () => G.has('gear');
  G.on('bossWin', (n, first) => {
    if (!on()) return;
    M.give(M.make(n / 50, first ? 1 : 0));
    if (Math.random() < 0.25 * G.st.drop) { M.giveRune(U.pick(RUNE_IDS), 1); }
    if (Math.random() < 0.12 * G.st.drop) { G.add('scrolls', 1); G.toast('📜 +1 защитный свиток'); }
  });
  G.on('chest', (loot) => {
    if (!on()) return;
    const tier = 1 + G.byId.stage.worlds();
    if (Math.random() < 0.35 * G.st.drop) { const it = M.make(tier); M.give(it, true); loot.list.push(`<span style="color:${RAR[it.r].c}">${SLOTS[it.slot][1]} ${it.name}</span>`); }
    if (Math.random() < 0.15 * G.st.drop) { M.giveRune(U.pick(RUNE_IDS), 1); loot.list.push('🔮 руна'); }
    if (Math.random() < 0.1 * G.st.drop) { G.add('scrolls', 1); loot.list.push('📜 свиток'); }
    const sc = U.ri(2, 8) * tier;
    G.add('scrap', sc); loot.list.push(`⚙️ ${sc}`);
  });
  G.on('golden', () => { if (on() && Math.random() < 0.03 * G.st.drop) M.give(M.make(1 + G.byId.stage.worlds())); });

  /* ---------- Панель ---------- */
  const itemTile = (it, extra = '') => `<button class="itile${sel === it.id ? ' sel' : ''}" style="--rc:${RAR[it.r].c}" data-a="gear.sel:${it.id}" data-k="i${it.id}">
    <span>${SLOTS[it.slot][1]}</span>${it.ench ? `<b>+${it.ench}</b>` : ''}${extra}</button>`;

  function render() {
    const s = G.S.gear;
    if (!s.starter) { s.starter = true; M.give(M.make(1, 1, 'gloves'), true); }
    let h = UI.inTabs([['eq', '🧤 Снаряжение'], ['runes', '🔮 Руны']], s.tab, 'gear.tab');
    h += `<div class="row small dim" style="margin-bottom:6px"><span>⚙️ Обломки: <b>${U.fint(G.cur('scrap'))}</b></span><span>📜 Свитки: <b>${U.fint(G.cur('scrolls'))}</b></span><span>🎒 ${M.invList().length}/${MAX_INV}</span></div>`;
    if (s.tab === 'runes') return h + renderRunes();
    // экипированные слоты
    h += '<div class="eqrow">';
    for (const sl of SLOT_IDS) {
      const it = s.items[s.eq[sl]];
      h += it ? `<div class="eqs">${itemTile(it)}<small>${SLOTS[sl][0]}</small></div>`
        : `<div class="eqs"><div class="itile empty"><span>${SLOTS[sl][1]}</span></div><small>${SLOTS[sl][0]}</small></div>`;
    }
    h += '</div>';
    // выбранный предмет
    const it = s.items[sel];
    if (it) h += renderItem(it);
    // инвентарь
    const inv = M.invList().sort((a, b) => b.r - a.r || b.ench - a.ench);
    h += UI.sec('Инвентарь', inv.length ? 'нажмите на предмет' : 'пусто — предметы падают с боссов и сундуков');
    h += '<div class="igrid">' + inv.map((x) => itemTile(x)).join('') + '</div>';
    if (inv.some((x) => x.r <= 1 && !x.ench)) h += '<button class="btn wide" data-a="gear.meltLow">♻️ Переплавить обычные и необычные</button>';
    return h;
  }

  function renderItem(it) {
    const s = G.S.gear, R = RAR[it.r], m = M.mult(it);
    const equipped = s.eq[it.slot] === it.id;
    let h = `<div class="idet" style="--rc:${R.c}" data-k="idet${it.id}"><div class="row"><b style="color:${R.c}">${SLOTS[it.slot][1]} ${it.name}${it.ench ? ' +' + it.ench : ''}</b><small style="color:${R.c}">${R.name} · ур. ${Math.floor(it.tier)}</small></div>`;
    h += '<div class="istats">' + it.stats.map(([k, v]) => `<div>${G.statText(k, v * m)}</div>`).join('') + '</div>';
    if (it.sock.length) {
      h += '<div class="socks">';
      it.sock.forEach((rn, i) => {
        if (rn) { const [t, l] = rn.split('_'); h += `<button class="sock full" data-a="gear.unsock:${i}">${RUNES[t][1]}<small>${l}</small></button>`; }
        else h += `<button class="sock${sockPick === i ? ' on' : ''}" data-a="gear.sock:${i}">＋</button>`;
      });
      h += '<small class="dim">гнёзда для рун</small></div>';
      if (sockPick >= 0) {
        const keys = Object.keys(s.runes).filter((k) => s.runes[k] > 0).sort((a, b) => +b.split('_')[1] - +a.split('_')[1]);
        h += keys.length ? '<div class="minis">' + keys.map((k) => { const [t, l] = k.split('_'); return `<button class="mini ok" data-a="gear.insert:${k}">${RUNES[t][1]} ${l} · ${G.statText(RUNES[t][2], runeVal(t, +l))} (${s.runes[k]})</button>`; }).join('') + '</div>'
          : '<div class="cdesc">Нет рун. Руны выпадают с боссов, из сундуков и при переплавке.</div>';
      }
    }
    // зачарование
    if (it.ench < MAX_ENCH) {
      const c = enchCost(it), ok = G.can(c), p = enchChance(it.ench);
      h += `<div class="row ench"><span>Зачарование +${it.ench + 1}: шанс <b class="${p < 0.6 ? 'bad' : 'good'}">${U.pct(p, 0)}</b>${it.ench >= 10 ? ' · при неудаче −1' : ''}</span>
        ${it.ench >= 10 ? `<button class="mini${s.useScroll ? ' ok' : ''}" data-a="gear.scroll">📜 ${s.useScroll ? 'вкл' : 'выкл'}</button>` : ''}</div>`;
      h += `<div class="btnrow"><button class="btn pri" data-a="gear.ench:${it.id}"${U.dis(ok)}>✨ Зачаровать · ${Object.keys(c).map((k) => G.CUR[k].icon + ' ' + U.fmt(c[k])).join(' ')}</button></div>`;
    } else h += '<div class="center good">Максимальное зачарование!</div>';
    h += `<div class="btnrow">${equipped ? `<button class="btn" data-a="gear.unequip:${it.slot}">Снять</button>` : `<button class="btn mag" data-a="gear.equip:${it.id}">Надеть</button>`}
      ${equipped ? '' : `<button class="btn danger" data-a="gear.melt:${it.id}">♻️ Переплавить (+${scrapOf(it)} ⚙️)</button>`}</div></div>`;
    return h;
  }

  function renderRunes() {
    const s = G.S.gear;
    let h = UI.info('3 одинаковые руны объединяются в руну уровнем выше (макс. 10). Вставляйте руны в гнёзда предметов на вкладке «Снаряжение».');
    h += '<button class="btn wide" data-a="gear.mergeAll">🔮 Объединить всё возможное</button>';
    let any = false;
    for (const t of RUNE_IDS) {
      const [name, icon, stat] = RUNES[t];
      const cells = [];
      for (let l = 1; l <= 10; l++) {
        const k = t + '_' + l, n = s.runes[k] || 0;
        if (!n) continue;
        any = true;
        cells.push(`<button class="mini${n >= 3 && l < 10 ? ' ok' : ''}" data-a="gear.merge:${k}"${U.dis(n >= 3 && l < 10)}>${icon} ур.${l} ×${n}</button>`);
      }
      h += UI.card({ k: 'r' + t, icon, title: name, desc: `${G.statText(stat, runeVal(t, 1))} на ур.1 · ${G.statText(stat, runeVal(t, 10))} на ур.10`, extra: cells.length ? `<div class="minis">${cells.join('')}</div>` : '', cls: cells.length ? '' : 'cant' });
    }
    if (!any) h += UI.info('Рун пока нет.', 'dim');
    return h;
  }

  G.achCat('gear', 'Снаряжение', '🧤', 'Сила экипировки +25%', (a) => a.mul('gearPow', 1.25));
  G.achTier('gear', 'it', ['Собиратель', 'Барахольщик', 'Оружейник'], () => G.S.stats.items || 0, [1, 25, 100], (t) => `Получить ${t} предметов`);
  G.achTier('gear', 'en', ['Зачарователь', 'Мастер чар', 'Великий чародей'], () => Math.max(0, ...Object.values(G.S.gear.items).map((i) => i.ench)), [5, 10, 15], (t) => `Предмет +${t}`);
  G.achAdd('gear', 'myth', 'Мифическая находка', 'Получить мифический предмет', () => Object.values(G.S.gear.items).some((i) => i.r >= 5));
  G.achTier('gear', 'rm', ['Рунолог', 'Рунный мастер'], () => G.S.stats.runeMerges || 0, [1, 25], (t) => `Объединить руны ${t} раз`);
})();
