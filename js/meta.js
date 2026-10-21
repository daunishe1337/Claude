'use strict';
/* ============================================================
   meta.js — мета-множитель.
   Растёт от суммы уровней всех систем вместе: каждый уровень
   любого улучшения, здания, таланта, питомца и т.д. даёт +0.05%
   ко всему доходу. Панель показывает вклад каждой системы.
   ============================================================ */
(() => {
  let cache = 0, parts = [];

  const M = G.mod({
    id: 'meta',
    // сумма уровней всех модулей (кроме самого мета-модуля)
    total() {
      let n = 0;
      parts = [];
      for (const m of G.mods) {
        if (!m.levels || m === M) continue;
        const v = m.levels() || 0;
        if (v > 0) {
          const p = (m.panels && m.panels[0]) || { name: m.id, icon: '•' };
          parts.push({ name: p.name, icon: p.icon, v });
        }
        n += v;
      }
      cache = n;
      return n;
    },
    mult() { return 1 + 0.0005 * cache * G.st.metaK; },
    stats(acc) { this.total(); acc.mul('all', 1 + 0.0005 * cache * (G.st.metaK || 1)); },
    panels: [{
      id: 'meta', group: 'more', name: 'Мета-множитель', icon: '🧬', order: 30,
      unlock: () => cache >= 150,
      hint: '150 уровней во всех системах',
      render() {
        let h = `<div class="pcard"><div class="center dim">Сумма уровней всех систем</div><div class="big">${U.fint(cache)}</div>
          <div class="center">Мета-множитель: <b class="hl">${U.mul(M.mult())}</b> ко всему доходу</div>
          <div class="center dim">+0.05% за каждый уровень любого улучшения</div></div>`;
        h += UI.sec('Вклад систем');
        h += '<div class="stats">';
        parts.slice().sort((a, b) => b.v - a.v).forEach((p) => {
          h += `<div><span>${p.icon} ${p.name}</span><b>${U.fint(p.v)}</b></div>`;
        });
        return h + '</div>';
      },
    }],
  });
  G.defStat('metaK', 1);
  G.statRow(20, () => [['🧬 Мета-множитель', U.mul(M.mult())], ['🏅 Мастерство зданий', String(G.byId.buildings.mastery())]]);
})();
