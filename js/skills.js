'use strict';
/* ============================================================
   skills.js — навыки тапа: активные способности за ману.
   Взрыв тапов, цепная молния, сверхсила ×100, искажение
   времени, безумие пальцев. Каждый навык прокачивается.
   ============================================================ */
(() => {
  const SK = [
    { id: 'burst', name: 'Взрыв тапов', icon: '💥', mana: 30, cd: 60, cost: 1e7,
      desc: (l) => `Мгновенно ${20 + 6 * l} тапов с критами` },
    { id: 'chain', name: 'Цепная молния', icon: '⚡', mana: 40, cd: 90, cost: 1e8,
      desc: (l) => `${4 + l} разрядов по ${U.mul(25 * (1 + 0.2 * l))} тапа, по боссу ×3` },
    { id: 'surge', name: 'Сверхсила', icon: '✴️', mana: 60, cd: 180, cost: 1e9,
      desc: (l) => `Тап ×100 на ${(5 + 0.5 * l).toFixed(1)} с` },
    { id: 'warp', name: 'Искажение времени', icon: '⏳', mana: 50, cd: 300, cost: 1e10,
      desc: (l) => `Мгновенно ${U.time(60 + 30 * l)} дохода зданий` },
    { id: 'frenzy', name: 'Безумие пальцев', icon: '🖐️', mana: 40, cd: 240, cost: 1e11,
      desc: (l) => `Авто-тапер ×10 на ${10 + 2 * l} с` },
  ];
  const BY = {};
  SK.forEach((s) => (BY[s.id] = s));
  const R = 3.2, MAXL = 30;

  const M = G.mod({
    id: 'skills',
    reset: 0,
    SK,
    state: () => {
      const lv = {}, ready = {}, end = {};
      SK.forEach((s) => { lv[s.id] = 0; ready[s.id] = 0; end[s.id] = 0; });
      return { lv, ready, end, mana: 100 };
    },
    onReset(layer) { if (layer >= 1) SK.forEach((s) => { G.S.skills.lv[s.id] = 0; }); },
    maxMana() { return 100 + G.st.manaMax; },
    regen() { return 1 * G.st.manaRegen; },
    cd(s) { return s.cd * G.st.skillCd; },
    cdLeft(id) { return Math.max(0, (G.S.skills.ready[id] - Date.now()) / 1000); },
    active(id) { return Date.now() < G.S.skills.end[id]; },
    upCost(s) { return s.cost * Math.pow(R, G.S.skills.lv[s.id]); },
    levels() { return SK.reduce((a, s) => a + G.S.skills.lv[s.id], 0); },
    goals() { return G.has('skills') ? SK.filter((s) => G.S.skills.lv[s.id] < MAXL).map((s) => ({ icon: s.icon, name: s.name, cost: this.upCost(s) * G.st.cost })) : []; },
    tick(dt) {
      const s = G.S.skills;
      s.mana = Math.min(this.maxMana(), s.mana + this.regen() * dt);
    },
    offline(sec) { G.S.skills.mana = Math.min(this.maxMana(), G.S.skills.mana + this.regen() * sec); },
    stats(acc) {
      const sp = G.st.skillPow || 1;
      if (this.active('surge')) acc.add('tapTemp', 99 * sp);
      if (this.active('frenzy')) acc.mul('autoSpeed', 10 * sp);
    },

    cast(id) {
      const sk = BY[id], s = G.S.skills, l = s.lv[id];
      if (!sk || l < 1) return;
      if (this.cdLeft(id) > 0 || s.mana < sk.mana) { SFX.play('error'); G.toast(s.mana < sk.mana ? '🔮 Не хватает маны' : '⏱ Навык перезаряжается'); return; }
      s.mana -= sk.mana;
      s.ready[id] = Date.now() + this.cd(sk) * 1000;
      const sp = G.st.skillPow || 1;
      const orb = document.getElementById('orb').getBoundingClientRect();
      const cx = orb.left + orb.width / 2, cy = orb.top + orb.height / 2;
      if (id === 'burst') {
        const n = Math.round((20 + 6 * l) * sp);
        let sum = 0;
        for (let i = 0; i < n; i++) sum += G.doTap(null, null, true, 1).total;
        FX.burst(cx, cy, 60, ['#ff3df2', '#ffd34d', '#22e6ff'], 380, 4);
        FX.ring(cx, cy, '#ff3df2', 220);
        FX.float(cx, cy - 40, '💥 +' + U.fmt(sum), '#ff3df2', 34, 1.4);
        FX.shake(true);
        SFX.play('mega');
      } else if (id === 'chain') {
        const n = 4 + l;
        let sum = 0;
        for (let i = 0; i < n; i++) {
          const v = G.tapVal * 25 * (1 + 0.2 * l) * sp;
          sum += v;
          setTimeout(() => {
            const x = cx + U.rand(-orb.width * 0.6, orb.width * 0.6), y = cy + U.rand(-orb.height * 0.6, orb.height * 0.4);
            FX.burst(x, y, 16, ['#bfefff', '#22e6ff', '#ffffff'], 300, 2.5);
            FX.float(x, y, '⚡+' + U.fmt(v), '#9ff3ff', 22);
            if (i === 0) { FX.flash('rgba(160,240,255,.45)'); SFX.play('zap'); }
          }, i * 90);
        }
        G.earn(sum, 'skill');
        G.emit('skillDmg', sum * 2); // по боссу молния бьёт ×3 (×1 уже учтён в доходе)
      } else if (id === 'surge') {
        s.end.surge = Date.now() + (5 + 0.5 * l) * 1000;
        FX.flash('rgba(255,211,77,.4)'); FX.ring(cx, cy, '#ffd34d', 260);
        SFX.play('boost');
      } else if (id === 'warp') {
        const v = G.cps * (60 + 30 * l) * sp;
        G.earn(v, 'skill');
        FX.float(cx, cy - 30, '⏳ +' + U.fmt(v), '#a368ff', 32, 1.4);
        FX.flash('rgba(163,104,255,.35)');
        SFX.play('win');
      } else if (id === 'frenzy') {
        s.end.frenzy = Date.now() + (10 + 2 * l) * 1000;
        FX.flash('rgba(69,255,154,.3)');
        SFX.play('boost');
      }
      G.count('skills');
      G.emit('skill', id);
      G.dirty = true;
    },

    panels: [{
      id: 'skills', group: 'upg', name: 'Навыки', icon: '✨', order: 6,
      unlock: () => (G.S.life.coins || 0) >= 5e7,
      hint: '50M монет за всё время',
      render() {
        const s = G.S.skills;
        let h = UI.info(`🔮 Мана: <b>${Math.floor(s.mana)}/${M.maxMana()}</b> · +${M.regen().toFixed(1)}/с. Навыки применяются кнопками на экране тапа.`);
        for (const sk of SK) {
          const l = s.lv[sk.id];
          const c = M.upCost(sk) * G.st.cost, ok = G.cur('coins') >= c && l < MAXL;
          h += UI.card({
            k: 'sk' + sk.id, icon: sk.icon, title: sk.name, lv: l ? 'ур. ' + l : '',
            desc: `${sk.desc(Math.max(1, l))}<br>🔮 ${sk.mana} · ♻️ ${U.time(M.cd(sk))}${l ? '' : ' · <span class="hl">не изучен</span>'}`,
            btn: l >= MAXL ? '<button class="buy max" disabled>МАКС</button>'
              : `<button class="buy${ok ? ' ok' : ''}" data-a="skills.up:${sk.id}"${U.dis(ok)}><small>${l ? 'Улучшить' : 'Изучить'}</small><span>🪙 ${U.fmt(c)}</span></button>`,
            cls: ok ? 'can' : 'cant',
          });
        }
        return h;
      },
      badge: () => SK.some((sk) => G.S.skills.lv[sk.id] === 0 && G.cur('coins') >= M.upCost(sk) * G.st.cost),
    }],
    actions: {
      cast(id) { M.cast(id); },
      up(id) {
        const sk = BY[id], s = G.S.skills;
        if (s.lv[id] >= MAXL) return;
        if (!G.pay({ coins: M.upCost(sk) * G.st.cost })) { SFX.play('error'); return; }
        s.lv[id]++;
        G.emit('buy', 'skill', 1);
        SFX.play('buy');
      },
    },
  });
  G.defStat('manaMax', 0);
  G.defStat('manaRegen', 1);
  G.defStat('skillPow', 1);
  G.defStat('skillCd', 1);

  // мана и кнопки навыков на экране тапа
  UI.view('tapBottom', 20, () => {
    if (!G.has('skills')) return '';
    const s = G.S.skills;
    const learned = SK.filter((sk) => s.lv[sk.id] > 0);
    if (!learned.length) return '';
    let h = `<div class="skills">${UI.bar(s.mana / M.maxMana(), 'violet mana', '🔮 ' + Math.floor(s.mana) + '/' + M.maxMana())}<div class="skrow">`;
    for (const sk of learned) {
      const cdl = M.cdLeft(sk.id), act = M.active(sk.id);
      const ready = cdl <= 0 && s.mana >= sk.mana;
      const p = cdl > 0 ? cdl / M.cd(sk) : 0;
      h += `<button class="skb${act ? ' act' : ready ? ' rdy' : ' cd'}" data-a="skills.cast:${sk.id}" data-k="${sk.id}" style="--p:${p.toFixed(3)}">
        <span class="bi">${sk.icon}</span><span class="bt">${cdl > 0 ? (cdl >= 60 ? Math.ceil(cdl / 60) + 'м' : Math.ceil(cdl) + 'с') : sk.mana}</span></button>`;
    }
    return h + '</div></div>';
  });
})();
