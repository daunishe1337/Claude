'use strict';
/* ============================================================
   cosmetics.js — скины объекта тапа и фоны.
   Коллекция: каждый открытый скин/фон даёт маленький бонус
   (действуют все открытые), надетый меняет внешний вид.
   «Авто» — внешний вид текущей локации.
   ============================================================ */
(() => {
  // [id, название, иконка, бонус [стат, значение], способ получения {souls|ach|tower|season|daily|event}]
  const SKINS = [
    ['coin', 'Монета', '🪙', ['tap', 0.02], {}],
    ['cookie', 'Печенье', '🍪', ['cps', 0.03], { souls: 50 }],
    ['melon', 'Арбуз', '🍉', ['farm', 0.1], { souls: 60 }],
    ['donut', 'Пончик', '🍩', ['evReward', 0.05], { souls: 80 }],
    ['orb', 'Хрустальный шар', '🔮', ['skillPow', 0.05], { souls: 100 }],
    ['cat', 'Котик', '🐱', ['pet', 0.05], { souls: 150 }],
    ['gem', 'Алмаз', '💎', ['crystal', 0.02], { souls: 200 }],
    ['eye', 'Оберег', '🧿', ['drop', 0.05], { souls: 250 }],
    ['moon', 'Луна', '🌙', ['manaRegen', 0.1], { souls: 300 }],
    ['star', 'Звезда', '🌟', ['all', 0.02], { ach: 40 }],
    ['earth', 'Планета Земля', '🌍', ['cps', 0.05], { tower: 25 }],
    ['sun', 'Солнце', '☀️', ['all', 0.03], { season: 25 }],
    ['dice', 'Кубик удачи', '🎲', ['critCh', 0.01], { daily: 30 }],
    ['crown', 'Корона', '👑', ['all', 0.05], { daily: 100 }],
    ['pumpkin', 'Тыква', '🎃', ['souls', 0.05], { event: 1 }],
    ['snow', 'Снежинка', '❄️', ['boostDur', 0.05], { event: 1 }],
  ].map(([id, name, icon, b, how]) => ({ id, name, icon, b, how, kind: 'skin' }));
  const BGS = [
    ['neon', 'Неон', ['#1c0b40', '#05020f'], ['tap', 0.02], {}],
    ['sunset', 'Закат', ['#4a1a2a', '#140508'], ['cps', 0.02], { souls: 100 }],
    ['deep', 'Глубина', ['#04163a', '#01040f'], ['offline', 0.05], { souls: 120 }],
    ['aurora', 'Аврора', ['#0b3a2e', '#020f0c'], ['xp', 0.05], { souls: 150 }],
    ['matrix', 'Матрица', ['#062a10', '#010802'], ['critCh', 0.005], { souls: 200 }],
    ['cosmos', 'Космос', ['#22104a', '#05020f'], ['dust', 0.03], { season: 40 }],
    ['gold', 'Золото', ['#4a3a0a', '#140f02'], ['all', 0.03], { ach: 100 }],
    ['sakura', 'Сакура', ['#4a1a3a', '#12050f'], ['pet', 0.05], { event: 1 }],
  ].map(([id, name, c, b, how]) => ({ id, name, icon: '🖼️', c, b, how, kind: 'bg' }));
  const ALL = SKINS.concat(BGS);
  const BY = {};
  ALL.forEach((x) => (BY[x.id] = x));
  let tab = 'skin';

  const howText = (h) => h.souls ? `👻 ${h.souls}` : h.ach ? `${h.ach} достижений` : h.tower ? `Башня: этаж ${h.tower}` : h.season ? `Сезон: ур. ${h.season}` : h.daily ? `Серия наград: ${h.daily} дн.` : h.event ? 'Магазин ивента' : 'Бесплатно';

  const M = G.mod({
    id: 'cosmetics',
    reset: 9,
    ALL,
    state: () => ({ own: { coin: 1, neon: 1 }, skin: 'auto', bg: 'auto' }),
    owned: (id) => !!G.S.cosmetics.own[id],
    give(id) {
      const s = G.S.cosmetics;
      if (!BY[id] || s.own[id]) return false;
      s.own[id] = Date.now();
      G.toast(`🎨 Новый ${BY[id].kind === 'skin' ? 'скин' : 'фон'}: <b>${BY[id].icon} ${BY[id].name}</b>`, 'ach');
      G.dirty = true;
      return true;
    },
    // автоматическое получение по условиям
    check() {
      const st = G.S.stats;
      for (const x of ALL) {
        if (this.owned(x.id)) continue;
        const h = x.how;
        if (h.ach && G.byId.ach.count() >= h.ach) this.give(x.id);
        if (h.tower && G.S.tower && G.S.tower.best >= h.tower) this.give(x.id);
        if (h.daily && G.S.daily && G.S.daily.best >= h.daily) this.give(x.id);
        if (h.season && G.S.season && G.S.season.lv >= h.season) this.give(x.id);
      }
      void st;
    },
    stats(acc) { for (const id in G.S.cosmetics.own) { const x = BY[id]; if (x) G.applyStat(acc, x.b[0], x.b[1]); } },
    levels() { return Object.keys(G.S.cosmetics.own).length * 2; },
    panels: [{
      id: 'cosmetics', group: 'more', name: 'Скины', icon: '🎨', order: 52,
      unlock: () => (G.S.life.souls || 0) >= 30,
      hint: '30 душ за всё время',
      render() {
        const s = G.S.cosmetics;
        let h = UI.info(`Открыто: ${Object.keys(s.own).length}/${ALL.length}. Бонусы всех открытых скинов и фонов суммируются.`);
        h += UI.inTabs([['skin', '🪙 Объект'], ['bg', '🖼️ Фон']], tab, 'cosmetics.tab');
        const list = tab === 'skin' ? SKINS : BGS, cur = tab === 'skin' ? s.skin : s.bg;
        h += `<div class="card${cur === 'auto' ? ' sel' : ''}" data-k="auto"><div class="cico">🗺️</div><div class="cmain"><div class="ctitle">Авто</div><div class="cdesc">Как у текущей локации</div></div>
          <div class="cbtn">${cur === 'auto' ? '<button class="buy max" disabled>Выбран</button>' : `<button class="buy ok" data-a="cosmetics.wear:${tab}:auto">Выбрать</button>`}</div></div>`;
        h += '<div class="skins">';
        for (const x of list) {
          const own = M.owned(x.id), on = cur === x.id;
          const look = x.kind === 'bg' ? `<span class="bgprev" style="background:radial-gradient(circle at 50% 35%, ${x.c[0]}, ${x.c[1]})"></span>` : `<span>${x.icon}</span>`;
          let btn;
          if (own) btn = on ? '<button class="mini done" disabled>Надет</button>' : `<button class="mini ok" data-a="cosmetics.wear:${tab}:${x.id}">Надеть</button>`;
          else if (x.how.souls) btn = `<button class="mini${G.cur('souls') >= x.how.souls ? ' ok' : ''}" data-a="cosmetics.buy:${x.id}"${U.dis(G.cur('souls') >= x.how.souls)}>👻 ${x.how.souls}</button>`;
          else btn = `<span class="mini lock">${howText(x.how)}</span>`;
          h += `<div class="skin${own ? '' : ' locked'}${on ? ' on' : ''}" data-k="${x.id}">${look}<b>${x.name}</b><small>${G.statText(x.b[0], x.b[1])}</small>${btn}</div>`;
        }
        return h + '</div>';
      },
    }],
    actions: {
      tab(t) { tab = t; },
      wear(kind, id) {
        const s = G.S.cosmetics;
        if (id !== 'auto' && !M.owned(id)) return;
        if (kind === 'skin') s.skin = id; else s.bg = id;
        G.byId.stage.applyVisual(true);
        SFX.play('buy');
      },
      buy(id) {
        const x = BY[id];
        if (!x || M.owned(id) || !x.how.souls || !G.pay({ souls: x.how.souls })) { SFX.play('error'); return; }
        M.give(id);
        SFX.play('win');
      },
    },
  });
  // интеграция с визуалом локации
  G.skinIcon = () => { const s = G.S.cosmetics; return s && s.skin !== 'auto' && BY[s.skin] ? BY[s.skin].icon : null; };
  G.bgColors = () => { const s = G.S.cosmetics; return s && s.bg !== 'auto' && BY[s.bg] ? BY[s.bg].c : null; };
  G.on('second', () => M.check());
})();
