'use strict';
/* ============================================================
   research.js — исследования.
   Дерево из 32 узлов (8 ярусов по 4). Каждое исследование идёт
   в реальном времени (и офлайн), стоит ресурсы и души.
   Ярус открывается, когда изучены 2 узла предыдущего.
   Скорость: интеллект героя, библиотека, бонусы.
   ============================================================ */
(() => {
  // [название, иконка, стат, значение]; ярусы по 4 узла
  const TIERS = [
    [['Основы экономики', '📊', 'cps', 0.1], ['Тренировка пальцев', '👆', 'tap', 0.1], ['Картография', '🗺️', 'stageReq', 0.03], ['Зоология', '🐾', 'pet', 0.1]],
    [['Логистика', '🚚', 'cost', 0.02], ['Анатомия', '🫀', 'critCh', 0.01], ['Метеорология', '🌦️', 'evFreq', 0.1], ['Металлургия', '🔥', 'mine', 0.2]],
    [['Банковское дело', '🏦', 'bank', 0.25], ['Боевая тактика', '🎯', 'combat', 0.15], ['Агрономия', '🌾', 'farm', 0.2], ['Рунология', '🔮', 'gearPow', 0.1]],
    [['Автоматизация', '🤖', 'autoSpeed', 0.2], ['Высшая алхимия', '⚗️', 'potion', 0.2], ['Демонология', '😈', 'souls', 0.15], ['Астрономия', '🔭', 'dust', 0.1]],
    [['Квантовая экономика', '⚛️', 'all', 0.15], ['Нейросети', '🧠', 'skillPow', 0.25], ['Генетика', '🧬', 'petSlots', 1], ['Сейсмология', '🌋', 'mine', 0.5]],
    [['Хронометрия', '⏱️', 'boostCd', 0.1], ['Кристаллография', '💎', 'crystal', 0.15], ['Фортификация', '🏰', 'army', 0.25], ['Селекция', '🌱', 'farmSpeed', 0.3]],
    [['Финансовая сингулярность', '💹', 'cps', 0.5], ['Теория струн', '🎻', 'tap', 0.5], ['Дальние походы', '🧭', 'expTime', 0.2], ['Мета-анализ', '📈', 'metaK', 0.1]],
    [['Теория всего', '🌌', 'all', 0.5], ['Темпоральная механика', '⏳', 'offlineH', 6], ['Космология', '🪐', 'dust', 0.25], ['Вечность', '♾️', 'achPow', 0.25]],
  ];
  const NODES = [];
  TIERS.forEach((row, t) => row.forEach(([name, icon, stat, v], i) => NODES.push({ id: `r${t}_${i}`, t, name, icon, stat, v })));
  const BY = {};
  NODES.forEach((n) => (BY[n.id] = n));
  // особые эффекты: «Генетика» — слот питомца, «Дальние походы» — экспедиции быстрее
  const SPECIAL = { petSlots: (a, v) => a.add('petSlots', v), expTime: (a, v) => a.mul('expTime', 1 - v) };
  const timeOf = (n) => 60 * Math.pow(2.6, n.t) * (1 + n.t * 0.2);          // секунд до ускорений
  const costOf = (n) => {
    const k = Math.pow(3, n.t);
    const c = { wood: Math.round(30 * k), stone: Math.round(20 * k) };
    if (n.t >= 2) c.metal = Math.round(5 * k / 3);
    if (n.t >= 4) c.gems = Math.round(2 * k / 27);
    if (n.t >= 6) c.ether = Math.round(k / 243);
    c.souls = Math.round(3 * Math.pow(2, n.t));
    return c;
  };

  const M = G.mod({
    id: 'research',
    reset: 3,
    NODES,
    state: () => ({ done: {}, cur: null, prog: 0 }),
    speed() { return G.st.research; },
    tierOpen(t) { return t === 0 || TIERS[t - 1].filter((_, i) => G.S.research.done[`r${t - 1}_${i}`]).length >= 2; },
    stats(acc) {
      for (const id in G.S.research.done) {
        const n = BY[id];
        if (!n) continue;
        if (SPECIAL[n.stat]) SPECIAL[n.stat](acc, n.v); else G.applyStat(acc, n.stat, n.v);
      }
    },
    levels() { return Object.keys(G.S.research.done).length * 3; },
    advance(sec) {
      const s = G.S.research;
      if (!s.cur) return;
      s.prog += sec * this.speed();
      const n = BY[s.cur];
      if (s.prog >= timeOf(n)) {
        s.done[n.id] = Date.now();
        s.cur = null; s.prog = 0;
        G.count('researched');
        G.toast(`🔬 Исследование завершено: <b>${n.icon} ${n.name}</b>`, 'ach');
        SFX.play('level');
        G.dirty = true;
      }
    },
    tick(dt) { this.advance(dt); },
    offline(sec) { this.advance(sec); },
    panels: [{
      id: 'research', group: 'more', name: 'Исследования', icon: '🔬', order: 49,
      unlock: () => G.has('city') && G.S.city.lv.library >= 1,
      hint: 'Постройте библиотеку в городе',
      render() {
        const s = G.S.research;
        let h = '';
        if (s.cur) {
          const n = BY[s.cur], T = timeOf(n);
          h += `<div class="pcard"><div class="row"><b>${n.icon} ${n.name}</b><span class="dim">осталось ${U.time((T - s.prog) / M.speed())}</span></div>${UI.bar(s.prog / T, 'violet', U.pct(s.prog / T, 0))}</div>`;
        } else h += UI.info(`Выберите исследование. Скорость: ${U.mul(M.speed())}. Изучено ${Object.keys(s.done).length}/${NODES.length}.`);
        h += '<div class="resrow">' + ['wood', 'stone', 'metal', 'gems', 'ether', 'souls'].map((id) => `<span>${G.CUR[id].icon} ${U.fint(G.cur(id))}</span>`).join('') + '</div>';
        TIERS.forEach((row, t) => {
          const open = M.tierOpen(t);
          h += UI.sec(`Ярус ${t + 1}`, open ? '' : '🔒 изучите 2 узла предыдущего яруса');
          if (!open) return;
          row.forEach((_, i) => {
            const n = BY[`r${t}_${i}`], done = s.done[n.id], cur = s.cur === n.id, c = costOf(n);
            const eff = SPECIAL[n.stat] ? (n.stat === 'petSlots' ? '+1 слот питомца' : `Экспедиции быстрее на ${U.pct(n.v, 0)}`) : G.statText(n.stat, n.v);
            h += UI.card({
              k: n.id, icon: n.icon, title: n.name, desc: `${eff}<br>${done ? '✅ изучено' : `⏱ ${U.time(timeOf(n) / M.speed())} · ${UI.costs(c)}`}`,
              btn: done ? '' : cur ? '<button class="buy" disabled>Идёт…</button>' : UI.buyC('research.start:' + n.id, c, 'Изучить', !s.cur),
              cls: done ? 'done' : cur ? 'sel' : !s.cur && G.can(c) ? 'can' : 'cant',
            });
          });
        });
        return h;
      },
      badge: () => !G.S.research.cur && NODES.some((n) => !G.S.research.done[n.id] && M.tierOpen(n.t) && G.can(costOf(n))),
    }],
    actions: {
      start(id) {
        const s = G.S.research, n = BY[id];
        if (!n || s.cur || s.done[id] || !M.tierOpen(n.t) || !G.pay(costOf(n))) { SFX.play('error'); return; }
        s.cur = id; s.prog = 0;
        SFX.play('open');
      },
    },
  });
  void M;
  G.achCat('research', 'Наука', '🔬', 'Исследования ×1.5', (a) => a.mul('research', 1.5));
  G.achTier('research', 'rs', ['Лаборант', 'Учёный', 'Профессор', 'Академик'], () => G.S.stats.researched || 0, [1, 8, 20, 32], (t) => `Завершить ${t} исследований`);
})();
