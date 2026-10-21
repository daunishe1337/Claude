'use strict';
/* ============================================================
   events.js — случайные события на экране тапа:
   ✨ золотой объект (клад / лихорадка / тап-шторм),
   📦 сундук (открыть несколькими тапами),
   ☄️ метеорит (поймать на лету).
   Прокачка частоты, размера награды и времени жизни.
   ============================================================ */
(() => {
  const UPG = {
    freq: { name: 'Частота событий', icon: '⏱️', base: 3000, r: 1.8, max: 25, val: (l) => U.mul(1 + 0.1 * l) },
    rew: { name: 'Размер награды', icon: '💎', base: 5000, r: 1.7, max: 40, val: (l) => U.mul(1 + 0.2 * l) },
    life: { name: 'Время жизни', icon: '⌛', base: 4000, r: 1.9, max: 15, desc: 'Как долго событие остаётся на экране', val: (l) => U.mul(1 + 0.15 * l) },
  };
  // типы событий: интервал появления (с), требование открытия
  const TYPES = {
    golden: { name: 'Золотой объект', icon: '✨', min: 80, max: 170, need: 1000 },
    chest: { name: 'Сундук', icon: '📦', min: 280, max: 520, need: 1e6 },
    meteor: { name: 'Метеорит', icon: '☄️', min: 220, max: 460, need: 1e7 },
  };
  const timers = { golden: 25, chest: 120, meteor: 200 };

  const M = G.mod({
    id: 'events',
    reset: 0,
    UPG, TYPES,
    state: () => ({ freq: 0, rew: 0, life: 0, fx: {} }),
    onReset(layer) { if (layer >= 1) { const s = G.S.events; s.freq = s.rew = s.life = 0; } },
    levels() { const s = G.S.events; return s.freq + s.rew + s.life; },
    goals() { return G.has('events') ? G.upgGoals('events', UPG, G.S.events) : []; },
    open: (t) => (G.S.life.coins || 0) >= TYPES[t].need,
    freq() { return (1 + 0.1 * G.S.events.freq) * G.st.evFreq; },
    rew() { return (1 + 0.2 * G.S.events.rew) * G.st.evReward; },
    lifeT() { return (1 + 0.15 * G.S.events.life) * G.st.evLife; },
    fxOn(id) { return Date.now() < (G.S.events.fx[id] || 0); },
    stats(acc) {
      if (this.fxOn('frenzy')) acc.add('allTemp', 6);
      if (this.fxOn('storm')) acc.add('tapTemp', 24);
    },
    tick(dt) {
      if (UI.tab !== 'tap' || document.hidden) return;   // события только на экране тапа
      for (const t in TYPES) {
        if (!this.open(t)) continue;
        timers[t] -= dt * this.freq();
        if (timers[t] <= 0) {
          timers[t] = U.rand(TYPES[t].min, TYPES[t].max);
          spawn(t);
        }
      }
    },
    panels: [{
      id: 'events', group: 'upg', name: 'События', icon: '✨', order: 7,
      unlock: () => (G.S.life.coins || 0) >= 2000,
      hint: '2K монет за всё время',
      render() {
        const st = G.S.stats;
        let h = UI.amt();
        h += UI.info(`Частота ${U.mul(M.freq())} · награды ${U.mul(M.rew())}<br>
          Поймано: ✨ ${U.fint(st.golden || 0)} · 📦 ${U.fint(st.chests || 0)} · ☄️ ${U.fint(st.meteors || 0)}`);
        for (const t in TYPES) if (!M.open(t)) h += UI.info(`🔒 ${TYPES[t].icon} ${TYPES[t].name} — при ${U.fmt(TYPES[t].need)} монет за всё время`, 'dim');
        for (const k in UPG) h += UI.upg('events', k, UPG[k], G.S.events[k]);
        return h;
      },
    }],
    actions: { upg(k) { G.buyUpg('events', k, UPG[k], G.S.events); } },
    // награда для внешних модулей (колесо, квесты): N секунд дохода
    secs(n) { return Math.max(10, G.income() * n * this.rew()); },
  });
  G.defStat('evReward', 1);
  G.defStat('evLife', 1);

  /* ---------- Появление событий ---------- */
  function layerBox() { return document.getElementById('evLayer'); }
  function place(el) {
    el.style.left = U.rand(8, 78) + '%';
    el.style.top = U.rand(6, 70) + '%';
  }
  function expire(el, sec) {
    setTimeout(() => { if (el.isConnected) { el.classList.add('gone'); setTimeout(() => el.remove(), 400); } }, sec * 1000);
  }

  function spawn(t) {
    const L = layerBox();
    if (L.querySelector('.ev-' + t)) return;
    const el = document.createElement('button');
    el.className = 'ev ev-' + t;
    if (t === 'golden') {
      el.innerHTML = '<span>🌟</span>';
      place(el);
      el.addEventListener('pointerdown', (e) => { e.preventDefault(); e.stopPropagation(); el.remove(); golden(e.clientX, e.clientY); });
      expire(el, 12 * M.lifeT());
    } else if (t === 'chest') {
      let hp = 5;
      el.innerHTML = '<span>📦</span><i>5</i>';
      place(el);
      el.addEventListener('pointerdown', (e) => {
        e.preventDefault(); e.stopPropagation();
        hp--;
        SFX.play('tap', 30);
        FX.burst(e.clientX, e.clientY, 6, ['#c98a3a', '#ffd34d']);
        el.querySelector('i').textContent = hp;
        el.animate([{ transform: 'rotate(-12deg)' }, { transform: 'rotate(12deg)' }, { transform: 'rotate(0)' }], { duration: 160 });
        if (hp <= 0) { el.remove(); chest(e.clientX, e.clientY); }
      });
      expire(el, 20 * M.lifeT());
    } else if (t === 'meteor') {
      el.innerHTML = '<span>☄️</span>';
      el.style.left = U.rand(-5, 40) + '%';
      el.style.top = '-12%';
      el.style.animationDuration = (5 * M.lifeT()).toFixed(1) + 's';
      el.addEventListener('pointerdown', (e) => { e.preventDefault(); e.stopPropagation(); el.remove(); meteor(e.clientX, e.clientY); });
      el.addEventListener('animationend', () => el.remove());
    }
    L.appendChild(el);
    G.emit('evSpawn', t);
  }

  /* ---------- Награды ---------- */
  function golden(x, y) {
    G.count('golden');
    SFX.play('coin');
    FX.burst(x, y, 30, ['#ffd34d', '#fff3b0', '#ffb020'], 300, 3.5);
    FX.ring(x, y, '#ffd34d', 160);
    const r = Math.random();
    const s = G.S.events;
    if (r < 0.55) {
      const v = M.secs(60);
      G.earn(v, 'event');
      FX.float(x, y, '🌟 Клад! +' + U.fmt(v), '#ffd34d', 26, 1.4);
    } else if (r < 0.77) {
      s.fx.frenzy = Date.now() + 30000 * M.lifeT();
      FX.float(x, y, '🌟 Лихорадка: доход ×7!', '#ffd34d', 24, 1.6);
      G.toast('🌟 Лихорадка: весь доход ×7 на 30 с!', 'good');
    } else if (r < 0.93) {
      s.fx.storm = Date.now() + 15000 * M.lifeT();
      FX.float(x, y, '🌟 Тап-шторм: тап ×25!', '#ffd34d', 24, 1.6);
      G.toast('🌟 Тап-шторм: тап ×25 на 15 с!', 'good');
    } else {
      const v = M.secs(240);
      G.earn(v, 'event');
      FX.float(x, y, '🌟 Сокровище! +' + U.fmt(v), '#ff3df2', 28, 1.6);
    }
    G.emit('golden');
    G.dirty = true;
  }

  // добыча сундука: другие модули добавляют свои предметы через событие 'chest'
  function chest(x, y) {
    G.count('chests');
    SFX.play('open');
    FX.burst(x, y, 40, ['#ffd34d', '#c98a3a', '#ffffff'], 340, 3.5);
    const loot = { coins: M.secs(300), list: [] };
    G.emit('chest', loot);
    G.earn(loot.coins, 'event');
    FX.float(x, y, '📦 +' + U.fmt(loot.coins), '#ffd34d', 26, 1.5);
    G.toast(`📦 Сундук: 🪙 ${U.fmt(loot.coins)}${loot.list.length ? ' · ' + loot.list.join(' · ') : ''}`, 'good');
  }

  function meteor(x, y) {
    G.count('meteors');
    SFX.play('zap');
    FX.flash('rgba(255,140,60,.35)');
    FX.burst(x, y, 50, ['#ff8a3d', '#ffd34d', '#ff3df2'], 380, 3.5);
    FX.shake(true);
    const loot = { coins: M.secs(180), list: [] };
    G.emit('meteor', loot);
    G.earn(loot.coins, 'event');
    FX.float(x, y, '☄️ +' + U.fmt(loot.coins), '#ff8a3d', 28, 1.5);
    if (loot.list.length) G.toast(`☄️ Метеорит: ${loot.list.join(' · ')}`, 'good');
  }

  // активные эффекты событий — значки над объектом тапа
  UI.view('tapTop', 5, () => {
    let h = '';
    if (M.fxOn('frenzy')) h += `<span class="buff" data-k="fr">🌟 ×7 ${Math.ceil((G.S.events.fx.frenzy - Date.now()) / 1000)}с</span>`;
    if (M.fxOn('storm')) h += `<span class="buff" data-k="stm">👆 ×25 ${Math.ceil((G.S.events.fx.storm - Date.now()) / 1000)}с</span>`;
    return h;
  });
})();
