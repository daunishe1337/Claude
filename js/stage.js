'use strict';
/* ============================================================
   stage.js — этапы, боссы и локации.
   • Заработанные монеты продвигают этапы;
   • каждые 50 этапов — босс с HP и таймером: урон наносят тапы,
     доход зданий и навыки. Победа даёт души (редкую валюту)
     и множитель дохода на текущий забег;
   • 10 миров со своим фоном, объектом тапа и бонусом.
   ============================================================ */
(() => {
  G.defCur('souls', 'Души', '👻', 3, true);

  // [название, значок, объект, босс, цвета (фон1, фон2, шар1, шар2, свечение), бонус, эффект]
  const LOCS = [
    ['Лес', '🌲', '🍄', '👹', ['#0f3a26', '#04120b', '#c6ff8a', '#2f9d2c', 'rgba(120,255,120,.45)'], '+25% к доходу зданий', (a) => a.mul('cps', 1.25)],
    ['Пустыня', '🏜️', '🏺', '🦂', ['#4a3410', '#140c04', '#ffe08a', '#d08a1c', 'rgba(255,200,80,.5)'], '+5% к шансу крита', (a) => a.add('critCh', 0.05)],
    ['Океан', '🌊', '🐚', '🦑', ['#0b2e5a', '#030c1c', '#9ff3ff', '#1c7fd0', 'rgba(80,200,255,.5)'], '+50% к бонусам питомцев', (a) => a.mul('pet', 1.5)],
    ['Горы', '⛰️', '💎', '🗿', ['#2c2f45', '#0a0b14', '#d7e6ff', '#6f7fa8', 'rgba(180,200,255,.45)'], '+50% к ресурсам шахты', (a) => a.mul('mine', 1.5)],
    ['Вулкан', '🌋', '🔥', '🐉', ['#4a120a', '#140302', '#ffb070', '#d0381c', 'rgba(255,100,40,.55)'], '×2 урон по боссам', (a) => a.mul('bossDmg', 2)],
    ['Ледник', '❄️', '🧊', '☃️', ['#1c3a4a', '#050f16', '#e8fbff', '#5fb8d8', 'rgba(160,230,255,.5)'], '+50% к длительности бустеров', (a) => a.mul('boostDur', 1.5)],
    ['Небеса', '☁️', '🌤️', '👼', ['#34346a', '#0c0c20', '#fffbe0', '#c8b8ff', 'rgba(255,250,200,.5)'], '+50% к опыту героя', (a) => a.mul('xp', 1.5)],
    ['Преисподняя', '🔥', '💀', '😈', ['#3a0a1e', '#100208', '#ff9ab0', '#a01040', 'rgba(255,60,120,.5)'], '+50% к душам', (a) => a.mul('souls', 1.5)],
    ['Космос', '🪐', '🪐', '👾', ['#1a0f3a', '#04020e', '#e0c8ff', '#6a3dd8', 'rgba(170,120,255,.55)'], '+25% к кристаллам престижа', (a) => a.mul('crystal', 1.25)],
    ['Бездна', '🌀', '🌀', '👁️', ['#06262a', '#000405', '#7affef', '#0a8a8a', 'rgba(60,255,230,.5)'], '×2 весь доход', (a) => a.mul('all', 2)],
  ].map(([name, icon, obj, boss, c, bonus, st], i) => ({ i, name, icon, obj, boss, c, bonus, st }));

  const BOSS_EVERY = 50;
  const isBoss = (n) => n % BOSS_EVERY === 0;
  let fight = null;      // { hp, max, end, total } — текущий бой
  let lastIcon = '';

  const M = G.mod({
    id: 'stage',
    reset: 0,
    LOCS,
    state: () => ({ stage: 1, prog: 0, wins: 0, cd: 0, maxEver: 1, loc: 0, auto: true, firsts: {} }),
    // на престиже сбрасываются этап и множитель боссов, рекорд и миры остаются
    onReset(layer) {
      const s = G.S.stage;
      s.stage = 1; s.prog = 0; s.wins = 0; s.cd = 0;
      fight = null;
      if (layer >= 3) { s.maxEver = 1; s.loc = 0; s.firsts = {}; }
      applyVisual(true);
    },
    req(n) { return 2000 * Math.pow(1.11, n - 1) * G.st.stageReq; },
    bossHp(n) { return this.req(n) * 8 * G.st.bossHp; },
    bossTime() { return 30 + G.st.bossTime; },
    worlds() { return Math.min(LOCS.length, 1 + Math.floor((G.S.stage.maxEver - 1) / BOSS_EVERY)); },
    world() { return Math.min(LOCS.length - 1, Math.floor((G.S.stage.stage - 1) / BOSS_EVERY)); },
    loc() { return LOCS[Math.min(G.S.stage.loc, this.worlds() - 1)]; },
    fighting: () => !!fight,
    soulsFor(n) { return Math.round((2 + 2 * (n / BOSS_EVERY)) * G.st.souls); },

    stats(acc) {
      const s = G.S.stage;
      if (s.wins) acc.mul('all', Math.pow(1.15 * G.st.bossMultK, s.wins));
      // каждый открытый мир: +5% ко всему доходу
      acc.mul('all', 1 + 0.05 * (this.worlds() - 1));
      this.loc().st(acc);
      // непотраченные души: +1% к доходу каждая
      acc.mul('all', 1 + 0.01 * G.cur('souls') * G.st.soulPow);
    },
    levels() { return Math.floor(G.S.stage.maxEver / 10); },

    // продвижение этапов заработанными монетами
    progress(v) {
      const s = G.S.stage;
      if (isBoss(s.stage)) return;
      s.prog += v;
      const r0 = this.req(s.stage);
      if (s.prog < r0) return;
      // сколько этапов пройдено сразу (сумма геометрической прогрессии 1.11^n)
      const toBoss = Math.ceil(s.stage / BOSS_EVERY) * BOSS_EVERY - s.stage;
      let k = Math.floor(Math.log(s.prog * 0.11 / r0 + 1) / Math.log(1.11));
      k = Math.max(1, Math.min(k, toBoss));
      s.prog = Math.max(0, s.prog - r0 * (Math.pow(1.11, k) - 1) / 0.11);
      s.stage += k;
      if (isBoss(s.stage)) s.prog = 0;
      if (s.stage > s.maxEver) {
        const before = this.worlds();
        s.maxEver = s.stage;
        if (this.worlds() > before) newWorld();
      }
      G.best('maxStage', s.stage);
    },

    startFight() {
      const s = G.S.stage;
      if (fight || !isBoss(s.stage) || Date.now() < s.cd) return;
      const max = this.bossHp(s.stage);
      fight = { hp: max, max, end: Date.now() + this.bossTime() * 1000 };
      SFX.play('boss');
      FX.shake(true);
      FX.flash('rgba(255,40,80,.35)');
      UI.actions.tab('tap');
      G.emit('bossStart', s.stage);
    },
    hit(v) {
      if (!fight) return;
      fight.hp -= v * G.st.bossDmg;
      if (fight.hp <= 0) win();
    },
    tick(dt) {
      if (!fight) return;
      // дополнительный урон (армия и др.) — доля дохода в секунду
      if (G.st.bossDps > 0) this.hit(G.cps * G.st.bossDps * dt);
      if (fight && Date.now() >= fight.end) lose();
    },
    farGoal() {
      const s = G.S.stage;
      if (isBoss(s.stage)) return { text: `${M.loc().boss} Победи босса этапа ${s.stage}`, eta: fight ? '⚔️' : '', prio: 2 };
      const next = Math.ceil(s.stage / BOSS_EVERY) * BOSS_EVERY;
      return { text: `${LOCS[Math.min(9, Math.floor((next - 1) / BOSS_EVERY))].boss} Босс этапа ${next}`, eta: `ещё ${next - s.stage} эт.`, prio: 2 };
    },

    panels: [{
      id: 'locs', group: 'more', name: 'Локации', icon: '🗺️', order: 20,
      unlock: () => G.S.stage.maxEver > BOSS_EVERY,
      hint: 'Победите первого босса',
      render() {
        const n = M.worlds(), s = G.S.stage;
        let h = UI.info(`Открыто миров: <b>${n}/${LOCS.length}</b> · каждый даёт +5% ко всему доходу (сейчас +${(n - 1) * 5}%).<br>Активный мир задаёт фон, объект тапа и особый бонус.`);
        h += `<div class="card" data-k="lauto"><div class="cmain"><div class="ctitle">Авто-переход</div><div class="cdesc">Переключаться на новый мир при открытии</div></div>
          <div class="cbtn"><button class="switch${s.auto ? ' on' : ''}" data-a="stage.auto"><i></i></button></div></div>`;
        LOCS.forEach((L, i) => {
          const open = i < n, cur = M.loc() === L;
          h += UI.card({
            k: 'loc' + i, icon: open ? L.icon : '🔒', title: open ? L.name : '???',
            desc: open ? `${L.obj} Бонус: <span class="hl">${L.bonus}</span><br>Босс: ${L.boss}` : `Откроется после босса этапа ${i * BOSS_EVERY}`,
            btn: open ? (cur ? '<button class="buy max" disabled>Выбран</button>' : `<button class="buy ok" data-a="stage.go:${i}">Отправиться</button>`) : '',
            cls: cur ? 'sel' : open ? '' : 'cant',
          });
        });
        return h;
      },
    }],
    actions: {
      fight() { M.startFight(); },
      go(i) { if (i < M.worlds()) { G.S.stage.loc = i; applyVisual(true); SFX.play('open'); } },
      auto() { G.S.stage.auto = !G.S.stage.auto; },
    },
    init() { applyVisual(true); },
  });
  G.defStat('stageReq', 1);  // множитель требований этапов (<1 — легче)
  G.defStat('bossHp', 1);
  G.defStat('bossDmg', 1);
  G.defStat('bossTime', 0);
  G.defStat('bossDps', 0);   // урон по боссу в долях дохода/с (армия)
  G.defStat('bossMultK', 1);
  G.defStat('souls', 1);
  G.defStat('soulPow', 1);
  G.defStat('xp', 1);
  G.defStat('pet', 1);
  G.defStat('mine', 1);
  G.defStat('crystal', 1);

  /* ---------- Бой ---------- */
  G.on('earn', (v, src) => {
    if (src !== 'offline') M.progress(v);
    if (fight && (src === 'tap' || src === 'cps' || src === 'skill' || src === 'rain')) M.hit(v);
  });
  G.on('skillDmg', (v) => M.hit(v));

  function win() {
    const s = G.S.stage, n = s.stage;
    const first = !s.firsts[n];
    s.firsts[n] = 1;
    const souls = M.soulsFor(n) * (first ? 3 : 1);
    G.add('souls', souls);
    s.wins++;
    s.stage++;
    s.prog = 0;
    fight = null;
    G.count('bossWins');
    if (s.stage > s.maxEver) {
      const before = M.worlds();
      s.maxEver = s.stage;
      if (M.worlds() > before) newWorld();
    }
    G.best('maxStage', s.stage);
    G.emit('bossWin', n, first);
    SFX.play('win');
    FX.flash('rgba(255,211,77,.45)');
    const r = document.getElementById('orb').getBoundingClientRect();
    FX.burst(r.left + r.width / 2, r.top + r.height / 2, 80, ['#ffd34d', '#ff3df2', '#22e6ff', '#fff'], 450, 4);
    G.dirty = true;
    UI.modal({
      title: '🏆 Босс повержен!',
      html: `<div class="center"><div style="font-size:60px">${M.loc().boss}</div>
        <p>Этап ${n} пройден${first ? ' <b class="hl">впервые</b>' : ''}!</p>
        <div class="big">+${souls} 👻</div>
        <p>Множитель боссов в этом забеге: <b class="hl">${U.mul(Math.pow(1.15 * G.st.bossMultK, s.wins))}</b></p></div>`,
      buttons: [{ text: 'Отлично!', cls: 'pri' }],
    });
    applyVisual();
  }
  function lose() {
    fight = null;
    G.S.stage.cd = Date.now() + 15000;
    G.count('bossFails');
    SFX.play('fail');
    G.toast('💨 Босс сбежал! Прокачайтесь и попробуйте снова.', 'bad');
    applyVisual();
  }
  function newWorld() {
    const i = M.worlds() - 1;
    const L = LOCS[i];
    G.toast(`🗺️ Открыт новый мир: <b>${L.icon} ${L.name}</b>!`, 'unlock');
    if (G.S.stage.auto) { G.S.stage.loc = i; applyVisual(true); }
    G.emit('world', i);
  }

  /* ---------- Внешний вид локации ---------- */
  function applyVisual(force) {
    const L = M.loc();
    const app = document.getElementById('app');
    if (!app) return;
    const bg = G.bgColors ? G.bgColors() : null;   // фон из коллекции скинов
    const key = L.i + (bg ? bg[0] : '');
    if (force || app.dataset.loc !== key) {
      app.dataset.loc = key;
      const [a0, b0, c, d, e] = L.c;
      const [a, b] = bg || [a0, b0];
      const st = app.style;
      st.setProperty('--loc1', a); st.setProperty('--loc2', b);
      st.setProperty('--orbA', c); st.setProperty('--orbB', d); st.setProperty('--glow', e);
    }
    const orb = document.getElementById('orb');
    const icon = fight ? L.boss : (G.skinIcon ? G.skinIcon() : null) || L.obj;
    orb.classList.toggle('boss', !!fight);
    if (icon !== lastIcon) { document.getElementById('orbIcon').textContent = icon; lastIcon = icon; }
  }
  M.applyVisual = applyVisual;

  /* ---------- Полоса этапа ---------- */
  UI.view('stageBar', 1, () => {
    const s = G.S.stage, L = M.loc();
    let h = `<div class="stage"><div class="st-row"><span>${L.icon} ${L.name} · <b>Этап ${s.stage}</b></span>`;
    h += s.wins ? `<span class="hl">${U.mul(Math.pow(1.15 * G.st.bossMultK, s.wins))} 👑</span>` : `<span class="dim">рекорд ${s.maxEver}</span>`;
    h += '</div>';
    if (fight) {
      const left = Math.max(0, (fight.end - Date.now()) / 1000);
      h += UI.bar(fight.hp / fight.max, 'red', `${L.boss} ${U.fmt(Math.max(0, fight.hp))} / ${U.fmt(fight.max)}`);
      h += UI.bar(left / M.bossTime(), 'gold thin', `⏱ ${left.toFixed(1)} с — тапайте!`);
    } else if (isBoss(s.stage)) {
      const cd = (s.cd - Date.now()) / 1000;
      h += cd > 0
        ? `<button class="btn wide bossbtn" disabled>${L.boss} Отдых босса ${Math.ceil(cd)} с</button>`
        : `<button class="btn wide bossbtn danger" data-a="stage.fight">⚔️ Сразиться с боссом ${L.boss} · HP ${U.fmt(M.bossHp(s.stage))}</button>`;
    } else {
      h += UI.bar(s.prog / M.req(s.stage), '', `${U.fmt(s.prog)} / ${U.fmt(M.req(s.stage))}`);
    }
    return h + '</div>';
  });
  G.on('second', () => applyVisual());
  G.on('frame', () => { if (fight) applyVisual(); });
})();
