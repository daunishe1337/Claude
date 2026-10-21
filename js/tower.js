'use strict';
/* ============================================================
   tower.js — башня испытаний.
   Бесконечные этажи, бой на общей арене. Рекорд даёт
   постоянный бонус +2% ко всему доходу за этаж, каждые
   10 этажей — звёзды для созвездий и шанс реликвии.
   ============================================================ */
(() => {
  const A = () => G.arena;
  const M = G.mod({
    id: 'tower',
    reset: 3,
    state: () => ({ best: 0, cd: 0, auto: false }),
    stats(acc) { acc.mul('all', 1 + 0.02 * G.S.tower.best); },
    levels() { return G.S.tower.best; },
    panels: [{
      id: 'tower', group: 'more', name: 'Башня', icon: '🗼', order: 48,
      unlock: () => G.S.hero.lv >= 12,
      hint: 'Уровень героя 12',
      render() {
        const s = G.S.tower, a = A();
        const next = s.best + 1;
        let h = `<div class="pcard"><div class="row"><b>🗼 Рекорд: этаж ${s.best}</b><span class="hl">+${2 * s.best}% ко всему доходу</span></div>
          <div class="row dim small"><span>⚔️ Сила ${U.fmt(a.playerPow())} · ❤️ ${U.fmt(a.playerHp())}</span><span>⭐ каждые 10 этажей</span></div></div>`;
        if (a.f && a.f.kind === 'tower') return h + a.html('tower.hit');
        const cd = (s.cd - Date.now()) / 1000;
        const foe = a.foe(next, 1.3);
        h += UI.card({ k: 'tnext', icon: foe.icon, title: `Этаж ${next}: ${foe.name}`, desc: `❤️ ${U.fmt(foe.hp)} · ⚔️ ${U.fmt(foe.atk)}`,
          btn: cd > 0 ? `<button class="buy" disabled>⏱ ${Math.ceil(cd)} с</button>` : `<button class="buy ok" data-a="tower.go"${U.dis(!a.f)}>Бой!</button>` });
        h += `<div class="card" data-k="tauto"><div class="cmain"><div class="ctitle">Подниматься дальше</div><div class="cdesc">После победы сразу начинать следующий этаж</div></div>
          <div class="cbtn"><button class="switch${s.auto ? ' on' : ''}" data-a="tower.auto"><i></i></button></div></div>`;
        return h;
      },
    }],
    actions: {
      go() {
        const s = G.S.tower;
        if (A().f || Date.now() < s.cd) return;
        A().start('tower', s.best + 1, 1.3);
        SFX.play('boss');
      },
      hit(node) { if (A().f && A().f.kind === 'tower') A().hit(node); },
      auto() { G.S.tower.auto = !G.S.tower.auto; },
    },
  });
  G.on('arena', (f, win) => {
    if (f.kind !== 'tower') return;
    const s = G.S.tower;
    if (win) {
      s.best = Math.max(s.best, f.floor);
      G.best('towerFloor', s.best);
      SFX.play('win');
      let extra = '';
      if (f.floor % 10 === 0) {
        const st = 1 + Math.floor(f.floor / 10);
        G.add('stars', st); extra = ` · ⭐ +${st}`;
        if (Math.random() < 0.03 * G.st.drop && G.byId.artifacts) G.byId.artifacts.giveRelic(null, `Награда ${f.floor} этажа башни`);
      }
      G.toast(`🗼 Этаж ${f.floor} покорён! +2% к доходу${extra}`, 'good');
      if (s.auto) setTimeout(() => { if (!A().f) M.actions.go(); }, 600);
    } else {
      s.cd = Date.now() + 20000;
      SFX.play('fail');
      G.toast(`🗼 Поражение на этаже ${f.floor}. Прокачайте героя и армию.`, 'bad');
    }
    G.dirty = true;
  });
  G.achTier('dungeon', 'tw', ['Восходящий', 'Альпинист', 'Покоритель башни', 'Небожитель'], () => G.S.tower.best, [10, 25, 50, 100], (t) => `Этаж башни ${t}`);
  G.titleAdd('tower', 'Покоритель башни', '🗼', () => G.S.tower.best >= 50, 'Боевая сила ×1.5, весь доход +15%', (a) => { a.mul('combat', 1.5); a.mul('all', 1.15); });
})();
