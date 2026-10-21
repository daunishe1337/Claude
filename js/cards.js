'use strict';
/* ============================================================
   cards.js — коллекционные карты.
   40 карт в 8 наборах. Паки покупаются за души (и выпадают из
   наград). Каждая карта — маленький бонус, дубликаты повышают
   уровень карты (до 5), полный набор даёт бонус набора.
   ============================================================ */
(() => {
  // набор: [название, иконка, карты [имя, иконка], бонус карты, бонус набора]
  const SETS = [
    ['Монеты', '🪙', [['Медяк', '🥉'], ['Серебро', '🥈'], ['Золото', '🥇'], ['Слиток', '🧈'], ['Сокровище', '💰']], ['all', 0.02], ['all', 0.25]],
    ['Здания', '🏗️', [['Хижина', '🛖'], ['Дом', '🏠'], ['Замок', '🏰'], ['Небоскрёб', '🏙️'], ['Станция', '🛰️']], ['cps', 0.03], ['cps', 0.5]],
    ['Монстры', '👾', [['Слизень', '🟢'], ['Гоблин', '👺'], ['Огр', '👹'], ['Демон', '😈'], ['Дракон', '🐉']], ['bossDmg', 0.04], ['bossDmg', 0.5]],
    ['Звери', '🦁', [['Мышь', '🐭'], ['Лиса', '🦊'], ['Медведь', '🐻'], ['Тигр', '🐯'], ['Единорог', '🦄']], ['pet', 0.03], ['pet', 0.3]],
    ['Стихии', '🔥', [['Вода', '💧'], ['Огонь', '🔥'], ['Земля', '🌍'], ['Воздух', '💨'], ['Молния', '⚡']], ['critCh', 0.003], ['critMul', 2]],
    ['Космос', '🌌', [['Луна', '🌙'], ['Комета', '☄️'], ['Сатурн', '🪐'], ['Солнце', '☀️'], ['Галактика', '🌌']], ['crystal', 0.02], ['crystal', 0.25]],
    ['Герои', '🦸', [['Рыцарь', '🛡️'], ['Лучник', '🏹'], ['Маг', '🧙'], ['Вор', '🗡️'], ['Король', '🤴']], ['xp', 0.04], ['combat', 0.5]],
    ['Легенды', '👑', [['Корона', '👑'], ['Грааль', '🏆'], ['Скипетр', '🔱'], ['Феникс', '🐦'], ['Бесконечность', '♾️']], ['all', 0.04], ['all', 0.5]],
  ].map(([name, icon, cards, cb, sb], i) => ({ i, name, icon, cards, cb, sb }));
  const CARDS = [];
  SETS.forEach((s) => s.cards.forEach(([name, icon], j) => CARDS.push({ id: s.i + '_' + j, set: s, j, name, icon, r: j < 2 ? 0 : j < 4 ? 1 : 2 })));
  const RW = [70, 25, 5];
  const RC = ['#b8b8c8', '#3aa0ff', '#ffb020'];
  const NEED = [0, 1, 2, 4, 8]; // дубликатов для следующего уровня

  const M = G.mod({
    id: 'cards',
    reset: 9,
    state: () => ({ own: {}, packs: 0, opened: 0 }),
    lv: (id) => (G.S.cards.own[id] ? G.S.cards.own[id].lv : 0),
    setDone(s) { return s.cards.every((_, j) => this.lv(s.i + '_' + j) > 0); },
    stats(acc) {
      for (const s of SETS) {
        let sum = 0;
        s.cards.forEach((_, j) => (sum += this.lv(s.i + '_' + j)));
        if (sum) G.applyStat(acc, s.cb[0], s.cb[1] * sum);
        if (this.setDone(s)) G.applyStat(acc, s.sb[0], s.sb[1]);
      }
    },
    levels() { let n = 0; for (const id in G.S.cards.own) n += G.S.cards.own[id].lv; return n; },
    packCost() { return 25; },
    // открыть пак: 5 карт
    open() {
      const out = [];
      for (let k = 0; k < 5; k++) {
        const r = U.wpick([0, 1, 2], (x) => RW[x] * (x ? G.st.drop : 1));
        const c = U.pick(CARDS.filter((x) => x.r === r));
        const o = G.S.cards.own[c.id] || (G.S.cards.own[c.id] = { lv: 0, dup: 0 });
        let tag = '';
        if (!o.lv) { o.lv = 1; tag = 'НОВАЯ'; }
        else if (o.lv < 5) { o.dup++; if (o.dup >= NEED[o.lv]) { o.dup -= NEED[o.lv]; o.lv++; tag = 'ур.' + o.lv; } else tag = '+1'; }
        else { G.add('souls', 2); tag = '+2👻'; }
        out.push({ c, tag });
      }
      G.S.cards.opened++;
      G.count('packs');
      return out;
    },
    panels: [{
      id: 'cards', group: 'more', name: 'Карты', icon: '🃏', order: 51,
      unlock: () => (G.S.life.souls || 0) >= 50 || G.S.cards.packs > 0,
      hint: '50 душ за всё время',
      render() {
        const s = G.S.cards;
        let h = `<div class="pcard"><div class="row"><span>🃏 Паков: <b>${s.packs}</b></span><span class="dim">карт ${Object.keys(s.own).length}/${CARDS.length}</span></div>
          <div class="btnrow"><button class="btn gold" data-a="cards.buy"${U.dis(G.cur('souls') >= M.packCost())}>Купить пак · 👻 ${M.packCost()}</button>
          <button class="btn mag" data-a="cards.open"${U.dis(s.packs > 0)}>Открыть</button></div></div>`;
        for (const st of SETS) {
          const done = M.setDone(st);
          h += UI.sec(`${st.icon} ${st.name}`, (done ? '✅ ' : '🎁 ') + G.statText(st.sb[0], st.sb[1]));
          h += '<div class="cgrid">';
          st.cards.forEach((_, j) => {
            const c = CARDS[st.i * 5 + j], o = s.own[c.id];
            h += o ? `<div class="ccard" style="--rc:${RC[c.r]}" data-k="${c.id}"><span>${c.icon}</span><b>${c.name}</b><small>${'★'.repeat(o.lv)}</small></div>`
              : `<div class="ccard none" style="--rc:${RC[c.r]}" data-k="${c.id}"><span>❔</span></div>`;
          });
          h += '</div>';
          h += `<div class="cdesc">Каждый уровень карт набора: ${G.statText(st.cb[0], st.cb[1])}</div>`;
        }
        return h;
      },
      badge: () => G.S.cards.packs > 0,
    }],
    actions: {
      buy() { if (!G.pay({ souls: M.packCost() })) { SFX.play('error'); return; } G.S.cards.packs++; SFX.play('buy'); },
      open() {
        if (G.S.cards.packs < 1) return;
        G.S.cards.packs--;
        const res = M.open();
        SFX.play('open');
        UI.modal({ title: '🃏 Пак карт', html: '<div class="pchips">' + res.map(({ c, tag }) => `<span class="pchip" style="--rc:${RC[c.r]}">${c.icon} ${c.name} <b>${tag}</b></span>`).join('') + '</div>', buttons: [{ text: 'Отлично', cls: 'pri' }] });
      },
    },
  });
  void M;
  // пак за открытие сундука (редко) и за боссов
  G.on('chest', (loot) => { if (G.has('cards') && Math.random() < 0.08) { G.S.cards.packs++; loot.list.push('🃏 пак'); } });
  G.on('bossWin', (n, first) => { if (G.has('cards') && first && n % 100 === 0) { G.S.cards.packs++; G.toast('🃏 +1 пак карт'); } });
  G.achCat('cards', 'Коллекции', '🃏', 'Добыча +25%', (a) => a.mul('drop', 1.25));
  G.achTier('cards', 'cd', ['Коллекционер карт', 'Картограф', 'Полная колода'], () => Object.keys(G.S.cards.own).length, [10, 25, 40], (t) => `Собрать ${t} карт`);
})();
