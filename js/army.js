'use strict';
/* ============================================================
   army.js — армия.
   Юниты нанимаются за ресурсы и автоматически сражаются:
   бьют боссов этапов (доля дохода в секунду), помогают в
   подземельях и башне. Прокачка урона, здоровья и численности.
   ============================================================ */
(() => {
  const UNITS = [
    { id: 'sword', name: 'Мечник', icon: '🗡️', atk: 1, hp: 10, cost: { wood: 10, metal: 2 } },
    { id: 'bow', name: 'Лучник', icon: '🏹', atk: 2, hp: 6, cost: { wood: 20, metal: 3 } },
    { id: 'mage', name: 'Маг', icon: '🧙', atk: 6, hp: 5, cost: { metal: 5, gems: 1 } },
    { id: 'knight', name: 'Рыцарь', icon: '🛡️', atk: 4, hp: 40, cost: { metal: 15, gems: 2 } },
    { id: 'dragon', name: 'Дракон', icon: '🐉', atk: 40, hp: 200, cost: { gems: 20, ether: 5, souls: 10 }, size: 5 },
  ];
  const UPG = {
    dmg: { name: 'Оружие', icon: '⚔️', max: 30, base: 5, r: 1.45, val: (l) => 'урон +' + 10 * l + '%' },
    hp: { name: 'Доспехи', icon: '🛡️', max: 30, base: 5, r: 1.45, val: (l) => 'здоровье +' + 10 * l + '%' },
    cap: { name: 'Плац', icon: '🚩', max: 30, base: 8, r: 1.5, val: (l) => 'лимит +' + 5 * l },
  };

  const M = G.mod({
    id: 'army',
    reset: 3,
    UNITS,
    state: () => { const n = {}; UNITS.forEach((u) => (n[u.id] = 0)); return { n, dmg: 0, hp: 0, cap: 0 }; },
    cap() { return 10 + 5 * G.S.army.cap + G.st.armyCap; },
    size() { return UNITS.reduce((a, u) => a + G.S.army.n[u.id] * (u.size || 1), 0); },
    power() { const s = G.S.army; return UNITS.reduce((a, u) => a + s.n[u.id] * u.atk, 0) * (1 + 0.1 * s.dmg) * G.st.army; },
    hpPool() { const s = G.S.army; return UNITS.reduce((a, u) => a + s.n[u.id] * u.hp, 0) * (1 + 0.1 * s.hp) * G.st.army; },
    upCost(k) { const d = UPG[k]; return Math.round(d.base * Math.pow(d.r, G.S.army[k])); },
    // урон по боссу: доля дохода в секунду с насыщением (макс. 50%)
    stats(acc) { const p = this.power(); if (p > 0) acc.add('bossDps', 0.5 * p / (p + 300)); },
    levels() { const s = G.S.army; return s.dmg + s.hp + s.cap + Math.floor(this.size() / 5); },
    panels: [{
      id: 'army', group: 'more', name: 'Армия', icon: '⚔️', order: 46,
      unlock: () => G.has('city') && G.S.city.lv.barracks >= 1,
      hint: 'Постройте казармы в городе',
      render() {
        const s = G.S.army;
        let h = `<div class="pcard"><div class="row"><span>⚔️ Сила армии: <b>${U.fmt(M.power())}</b></span><span>❤️ ${U.fmt(M.hpPool())}</span></div>
          <div class="row dim small"><span>Отряд ${M.size()}/${M.cap()}</span><span>урон по боссу: ${U.pct(G.st.bossDps, 1)} дохода/с</span></div>
          <div class="armyview">${UNITS.map((u) => s.n[u.id] ? `<span>${u.icon}×${s.n[u.id]}</span>` : '').join('')}</div></div>`;
        h += '<div class="resrow">' + ['wood', 'metal', 'gems', 'ether', 'souls'].map((id) => `<span>${G.CUR[id].icon} ${U.fint(G.cur(id))}</span>`).join('') + '</div>';
        h += UI.sec('Наём');
        for (const u of UNITS) {
          const room = M.size() + (u.size || 1) <= M.cap();
          h += UI.card({ k: 'u' + u.id, icon: u.icon, title: u.name, lv: s.n[u.id] ? '×' + s.n[u.id] : '', desc: `⚔️ ${u.atk} · ❤️ ${u.hp}${u.size ? ' · занимает ' + u.size + ' места' : ''}<br>${UI.costs(u.cost)}`,
            btn: UI.buyC('army.hire:' + u.id, u.cost, 'Нанять', room), cls: room && G.can(u.cost) ? 'can' : 'cant' });
        }
        h += UI.sec('Тренировка', 'за души');
        for (const k in UPG) {
          const d = UPG[k], l = s[k], max = l >= d.max, c = M.upCost(k);
          h += UI.card({ k: 'au' + k, icon: d.icon, title: d.name, lv: 'ур. ' + l, desc: `${d.val(l)}${max ? '' : ` → <span class="hl">${d.val(l + 1)}</span>`}`,
            btn: max ? '<button class="buy max" disabled>МАКС</button>' : UI.buyC('army.up:' + k, { souls: c }), cls: max ? 'done' : G.cur('souls') >= c ? 'can' : 'cant' });
        }
        return h;
      },
    }],
    actions: {
      hire(id) {
        const u = UNITS.find((x) => x.id === id);
        if (!u || M.size() + (u.size || 1) > M.cap() || !G.pay(u.cost)) { SFX.play('error'); return; }
        G.S.army.n[id]++;
        G.count('recruits');
        SFX.play('buy');
      },
      up(k) {
        const s = G.S.army;
        if (s[k] >= UPG[k].max || !G.pay({ souls: M.upCost(k) })) { SFX.play('error'); return; }
        s[k]++;
        SFX.play('buy');
      },
    },
  });
  void M;
  G.achTier('city', 'ar', ['Ополчение', 'Легион', 'Армада'], () => M.size(), [10, 50, 150], (t) => `Армия из ${t} бойцов`);
})();
