'use strict';
/* ============================================================
   dungeon.js — подземелья и общий боевой движок (арена).
   Бой: тапайте по врагу (урон = боевая сила героя), армия
   бьёт автоматически, враг атакует раз в 1.5 с. 30 секунд
   на этаж. Подземелье: вход за ключ, этажи подряд, жетоны и
   добыча; прокачка ключей, их восстановления и попыток.
   Башня испытаний (tower.js) использует ту же арену.
   ============================================================ */
(() => {
  G.defCur('tokens', 'Жетоны подземелий', '🎖️', 3);
  const FOES = ['👺', '🧟', '🕷️', '🐺', '🦇', '👻', '🧌', '🦂', '🐍', '🧛', '🗿', '👹', '🐲', '💀', '👾'];
  const FOE_NAMES = ['Гоблин', 'Зомби', 'Паук', 'Волк', 'Летучая мышь', 'Призрак', 'Тролль', 'Скорпион', 'Змей', 'Вампир', 'Голем', 'Демон', 'Дракон', 'Лич', 'Пришелец'];

  /* ---------- Арена ---------- */
  const A = {
    f: null, // { kind, floor, icon, name, hp, max, atk, php, pmax, end, nextAtk }
    playerPow() { return G.byId.hero.power() + G.byId.army.power() * 0.2; },
    playerHp() { return G.byId.hero.hp() + G.byId.army.hpPool(); },
    foe(floor, scale = 1) {
      const i = Math.min(FOES.length - 1, Math.floor((floor - 1) / 5) % FOES.length);
      return { icon: FOES[i], name: FOE_NAMES[i], hp: 60 * Math.pow(1.16, floor) * scale, atk: 4 * Math.pow(1.13, floor) * scale };
    },
    start(kind, floor, scale, keepHp) {
      const e = this.foe(floor, scale);
      const pmax = this.playerHp();
      // между этажами герой восстанавливает 30% здоровья
      this.f = { kind, floor, icon: e.icon, name: e.name, hp: e.hp, max: e.hp, atk: e.atk, pmax, php: keepHp ? Math.min(pmax, keepHp + 0.3 * pmax) : pmax, end: Date.now() + 30000, nextAtk: Date.now() + 1500 };
    },
    hit(node) {
      const f = this.f;
      if (!f) return;
      const crit = Math.random() < Math.min(0.5, G.st.critCh);
      const d = this.playerPow() * (crit ? 3 : 1);
      f.hp -= d;
      SFX.play(crit ? 'crit' : 'tap', 20);
      VIB(crit ? 20 : 8);
      if (node && node.getBoundingClientRect) {
        const r = node.getBoundingClientRect();
        FX.float(r.left + r.width / 2 + U.rand(-30, 30), r.top + r.height / 3, (crit ? '💥' : '-') + U.fmt(d), crit ? '#ffb020' : '#fff', crit ? 26 : 20, 0.7);
      }
      this.check();
    },
    tick(dt) {
      const f = this.f;
      if (!f) return;
      f.hp -= G.byId.army.power() * dt;
      const now = Date.now();
      while (f && now >= f.nextAtk) { f.php -= f.atk; f.nextAtk += 1500; }
      this.check();
      if (this.f && now >= this.f.end) this.finish(false);
    },
    check() {
      const f = this.f;
      if (!f) return;
      if (f.hp <= 0) this.finish(true);
      else if (f.php <= 0) this.finish(false);
    },
    finish(win) {
      const f = this.f;
      this.f = null;
      G.emit('arena', f, win);
      G.dirty = true;
    },
    // разметка арены
    html(act) {
      const f = this.f;
      if (!f) return '';
      const left = Math.max(0, (f.end - Date.now()) / 1000);
      return `<div class="arena"><div class="row"><b>Этаж ${f.floor}</b><span class="dim">${f.name}</span><span>⏱ ${left.toFixed(0)} с</span></div>
        ${UI.bar(f.hp / f.max, 'red', `${f.icon} ${U.fmt(Math.max(0, f.hp))} / ${U.fmt(f.max)}`)}
        <button class="foe" data-a="${act}"><span>${f.icon}</span></button>
        ${UI.bar(f.php / f.pmax, 'green', `❤️ ${U.fmt(Math.max(0, f.php))} / ${U.fmt(f.pmax)}`)}
        <div class="center dim small">Тап: ${U.fmt(this.playerPow())} урона · армия ${U.fmt(G.byId.army.power())}/с · враг бьёт ${U.fmt(f.atk)} раз в 1.5 с</div></div>`;
    },
  };
  G.arena = A;

  /* ---------- Подземелье ---------- */
  const UPG = {
    keys: { name: 'Связка ключей', icon: '🗝️', max: 7, base: 30, r: 1.8, val: (l) => `${3 + l} ключей макс.` },
    regen: { name: 'Кузнец ключей', icon: '⏱️', max: 8, base: 40, r: 1.9, val: (l) => `ключ раз в ${U.time(7200 * Math.pow(0.9, l))}` },
    lives: { name: 'Попытки', icon: '❤️‍🩹', max: 3, base: 80, r: 2.5, val: (l) => `${1 + l} попыт. за забег` },
  };
  const SHOP = [
    { id: 'egg', name: 'Яйцо питомца', icon: '🥚', cost: 30, give: () => G.add('eggs', 1) },
    { id: 'scroll', name: 'Защитный свиток', icon: '📜', cost: 20, give: () => G.add('scrolls', 1) },
    { id: 'rune', name: 'Руна 2 ур.', icon: '🔮', cost: 40, give: () => G.byId.gear.giveRune(U.pick(Object.keys(G.byId.gear.RUNES)), 2) },
    { id: 'food', name: 'Корм ×100', icon: '🍖', cost: 15, give: () => G.add('food', 100) },
    { id: 'key', name: 'Ключ', icon: '🗝️', cost: 50, give: () => { G.S.dungeon.keys++; } },
  ];

  const M = G.mod({
    id: 'dungeon',
    reset: 3,
    state: () => ({ keys: 3, keyAt: Date.now(), keysLv: 0, regen: 0, lives: 0, run: null, best: 0 }),
    maxKeys() { return 3 + G.S.dungeon.keysLv; },
    keyTime() { return 7200 * Math.pow(0.9, G.S.dungeon.regen) * 1000; },
    upCost(k) { const d = UPG[k], l = k === 'keys' ? G.S.dungeon.keysLv : G.S.dungeon[k]; return Math.round(d.base * Math.pow(d.r, l)); },
    regenKeys() {
      const s = G.S.dungeon;
      if (s.keys >= this.maxKeys()) { s.keyAt = Date.now(); return; }
      while (s.keys < this.maxKeys() && Date.now() - s.keyAt >= this.keyTime()) { s.keys++; s.keyAt += this.keyTime(); }
    },
    tick(dt) { A.tick(dt); this.regenKeys(); },
    offline() { this.regenKeys(); },
    levels() { const s = G.S.dungeon; return s.keysLv + s.regen + s.lives + Math.floor(s.best / 2); },
    reward(floor) {
      const got = [];
      const tk = Math.round((1 + floor / 4) * G.st.drop);
      G.add('tokens', tk); got.push(`🎖️ ${tk}`);
      if (floor % 5 === 0) {
        const sc = 5 * floor; G.add('scrap', sc); got.push(`⚙️ ${sc}`);
        if (Math.random() < 0.4) { G.byId.gear.giveRune(U.pick(Object.keys(G.byId.gear.RUNES)), 1 + Math.floor(floor / 20)); got.push('🔮 руна'); }
        if (Math.random() < 0.3) { G.add('scrolls', 1); got.push('📜'); }
      }
      if (floor % 10 === 0) {
        const it = G.byId.gear.make(floor / 3, floor >= 30 ? 2 : 1); G.byId.gear.give(it, true); got.push(`${G.byId.gear.SLOTS[it.slot][1]} ${it.name}`);
        if (Math.random() < 0.3) { G.add('eggs', 1); got.push('🥚'); }
      }
      if (floor >= 20 && Math.random() < 0.005 * G.st.drop && G.byId.artifacts) G.byId.artifacts.giveRelic(null, `Найдена на ${floor} этаже подземелья`);
      return got;
    },
    panels: [{
      id: 'dungeon', group: 'more', name: 'Подземелья', icon: '🏚️', order: 47,
      unlock: () => G.S.hero.lv >= 15,
      hint: 'Уровень героя 15',
      render() {
        const s = G.S.dungeon;
        if (s.run) {
          let h = UI.info(`🏚️ Забег: этаж <b>${s.run.floor}</b> · попыток осталось ${s.run.lives} · добыто 🎖️ ${s.run.tokens}`);
          h += A.f && A.f.kind === 'dungeon' ? A.html('dungeon.hit') : `<div class="btnrow"><button class="btn mag" data-a="dungeon.next">⚔️ Этаж ${s.run.floor}</button><button class="btn" data-a="dungeon.leave">🚪 Выйти</button></div>`;
          return h;
        }
        const left = s.keys < M.maxKeys() ? U.time((M.keyTime() - (Date.now() - s.keyAt)) / 1000) : 'полно';
        let h = `<div class="pcard"><div class="row"><span>🗝️ Ключи: <b>${s.keys}/${M.maxKeys()}</b></span><span class="dim">следующий: ${left}</span></div>
          <div class="row dim small"><span>⚔️ Сила ${U.fmt(A.playerPow())} · ❤️ ${U.fmt(A.playerHp())}</span><span>рекорд: этаж ${s.best}</span></div>
          <button class="btn mag wide" data-a="dungeon.enter"${U.dis(s.keys > 0)}>🏚️ Войти в подземелье</button></div>`;
        h += `<div class="row"><span>🎖️ Жетоны: <b>${U.fint(G.cur('tokens'))}</b></span></div>`;
        h += UI.sec('Лавка жетонов');
        h += '<div class="minis">' + SHOP.map((x) => `<button class="mini${G.cur('tokens') >= x.cost ? ' ok' : ''}" data-a="dungeon.shop:${x.id}"${U.dis(G.cur('tokens') >= x.cost)}>${x.icon} ${x.name} · 🎖️${x.cost}</button>`).join('') + '</div>';
        h += UI.sec('Улучшения', 'за жетоны');
        for (const k in UPG) {
          const d = UPG[k], l = k === 'keys' ? s.keysLv : s[k], max = l >= d.max, c = M.upCost(k);
          h += UI.card({ k: 'du' + k, icon: d.icon, title: d.name, lv: 'ур. ' + l, desc: `${d.val(l)}${max ? '' : ` → <span class="hl">${d.val(l + 1)}</span>`}`,
            btn: max ? '<button class="buy max" disabled>МАКС</button>' : UI.buyC('dungeon.up:' + k, { tokens: c }), cls: max ? 'done' : G.cur('tokens') >= c ? 'can' : 'cant' });
        }
        return h;
      },
      badge: () => G.S.dungeon.keys >= M.maxKeys() && !G.S.dungeon.run,
    }],
    actions: {
      enter() {
        const s = G.S.dungeon;
        if (s.keys < 1 || s.run || A.f) return;
        if (s.keys >= M.maxKeys()) s.keyAt = Date.now();
        s.keys--;
        s.run = { floor: 1, lives: 1 + s.lives, tokens: 0, php: 0 };
        A.start('dungeon', 1);
        SFX.play('boss');
      },
      next() { const s = G.S.dungeon; if (s.run && !A.f) A.start('dungeon', s.run.floor, 1, s.run.php); },
      hit(node) { if (A.f && A.f.kind === 'dungeon') A.hit(node); },
      leave() {
        const s = G.S.dungeon;
        if (!s.run) return;
        G.toast(`🚪 Забег окончен: этаж ${s.run.floor - 1}, 🎖️ ${s.run.tokens}`, 'good');
        s.run = null; A.f = null;
      },
      shop(id) {
        const x = SHOP.find((y) => y.id === id);
        if (!x || !G.pay({ tokens: x.cost })) { SFX.play('error'); return; }
        x.give(); SFX.play('buy');
      },
      up(k) {
        const s = G.S.dungeon, key = k === 'keys' ? 'keysLv' : k;
        if (s[key] >= UPG[k].max || !G.pay({ tokens: M.upCost(k) })) { SFX.play('error'); return; }
        s[key]++; SFX.play('buy');
      },
    },
  });

  // итог боя подземелья
  G.on('arena', (f, win) => {
    if (f.kind !== 'dungeon') return;
    const s = G.S.dungeon;
    if (!s.run) return;
    if (win) {
      const got = M.reward(f.floor);
      s.run.tokens += Math.round((1 + f.floor / 4) * G.st.drop);
      s.run.php = f.php;
      if (f.floor > s.best) s.best = f.floor;
      G.best('dungeonFloor', f.floor);
      G.count('dungeonFloors');
      s.run.floor++;
      SFX.play('win');
      G.toast(`✅ Этаж ${f.floor}: ${got.join(' · ')}`, 'good');
    } else {
      s.run.lives--;
      SFX.play('fail');
      if (s.run.lives > 0) { s.run.php = 0; G.toast(`💔 Поражение на этаже ${f.floor}. Осталось попыток: ${s.run.lives}`, 'bad'); }
      else { G.toast(`☠️ Забег окончен на этаже ${f.floor}. Добыто 🎖️ ${s.run.tokens}`, 'bad'); s.run = null; }
    }
  });
  G.achCat('dungeon', 'Подземелья и башня', '🏚️', 'Боевая сила +50%', (a) => a.mul('combat', 1.5));
  G.achTier('dungeon', 'dg', ['Спелеолог', 'Охотник за сокровищами', 'Покоритель глубин'], () => G.S.stats.dungeonFloor || 0, [5, 20, 50], (t) => `Этаж подземелья ${t}`);
})();
