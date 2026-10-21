'use strict';
/* ============================================================
   weather.js — погода и время суток.
   Время суток — по часам телефона: утром больше опыта, днём —
   дохода зданий, вечером — событий, ночью сильнее магия.
   Погода меняется каждые 15 минут: дождь ускоряет ферму,
   гроза усиливает криты и навыки, снег — шахту и т.д.
   ============================================================ */
(() => {
  const SLOT = 15 * 60000;
  const TIMES = [
    { id: 'morning', name: 'Утро', icon: '🌅', from: 6, to: 12, desc: 'Опыт +20%', st: (a) => a.mul('xp', 1.2) },
    { id: 'day', name: 'День', icon: '☀️', from: 12, to: 18, desc: 'Доход зданий +10%', st: (a) => a.mul('cps', 1.1) },
    { id: 'evening', name: 'Вечер', icon: '🌆', from: 18, to: 22, desc: 'События +20%', st: (a) => a.mul('evFreq', 1.2) },
    { id: 'night', name: 'Ночь', icon: '🌙', from: 22, to: 6, desc: 'Магия: навыки +50%, мана +50%', st: (a) => { a.mul('skillPow', 1.5); a.mul('manaRegen', 1.5); } },
  ];
  const WEATHER = [
    { id: 'clear', name: 'Ясно', icon: '☀️', w: 30, desc: 'Тап +10%', st: (a) => a.mul('tap', 1.1) },
    { id: 'clouds', name: 'Облачно', icon: '☁️', w: 18, desc: 'Бустеры +10% длительности', st: (a) => a.mul('boostDur', 1.1) },
    { id: 'rain', name: 'Дождь', icon: '🌧️', w: 18, desc: 'Ферма растёт ×1.5', st: (a) => a.mul('farmSpeed', 1.5), fx: 'rain' },
    { id: 'storm', name: 'Гроза', icon: '⛈️', w: 8, desc: 'Крит +5%, навыки +20%', st: (a) => { a.add('critCh', 0.05); a.mul('skillPow', 1.2); }, fx: 'storm' },
    { id: 'fog', name: 'Туман', icon: '🌫️', w: 10, desc: 'События ×1.3', st: (a) => a.mul('evFreq', 1.3), fx: 'fog' },
    { id: 'snow', name: 'Снег', icon: '❄️', w: 8, desc: 'Шахта ×1.3', st: (a) => a.mul('mine', 1.3), fx: 'snow' },
    { id: 'wind', name: 'Ветер', icon: '🌬️', w: 10, desc: 'Авто-тапер +20%', st: (a) => a.mul('autoSpeed', 1.2), fx: 'wind' },
    { id: 'rainbow', name: 'Радуга', icon: '🌈', w: 3, desc: 'Весь доход +15%', st: (a) => a.mul('all', 1.15) },
    { id: 'starfall', name: 'Звездопад', icon: '🌠', w: 4, night: true, desc: 'Награды событий +30%, звёзды с золотых объектов', st: (a) => a.mul('evReward', 1.3), fx: 'stars' },
  ];
  let shownFx = '';

  const timeOf = (t = Date.now()) => {
    const h = new Date(t).getHours();
    return TIMES.find((x) => (x.from < x.to ? h >= x.from && h < x.to : h >= x.from || h < x.to));
  };
  const weatherOf = (slot) => {
    const night = timeOf(slot * SLOT).id === 'night';
    const list = WEATHER.filter((w) => !w.night || night);
    return U.wpick(list, (w) => w.w, U.seeded(slot * 2654435761));
  };

  const M = G.mod({
    id: 'weather',
    slot: () => Math.floor(Date.now() / SLOT),
    now() { return weatherOf(this.slot()); },
    time: () => timeOf(),
    isNight: () => timeOf().id === 'night',
    stats(acc) {
      if (!G.has('weather')) return;
      this.time().st(acc);
      this.now().st(acc);
    },
    panels: [{
      id: 'weather', group: 'more', name: 'Погода', icon: '⛅', order: 53,
      unlock: () => (G.S.stats.prestiges || 0) >= 1 || G.S.hero.lv >= 6,
      hint: 'Уровень героя 6',
      render() {
        const w = M.now(), t = M.time();
        const left = (SLOT - (Date.now() % SLOT)) / 1000;
        let h = `<div class="pcard"><div class="row"><span style="font-size:42px">${w.icon}</span><div style="flex:1"><b>${w.name}</b><div class="cdesc">${w.desc}</div></div><span class="dim">ещё ${U.time(left)}</span></div>
          <div class="row"><span style="font-size:32px">${t.icon}</span><div style="flex:1"><b>${t.name}</b><div class="cdesc">${t.desc}</div></div><span class="dim">${t.from}:00–${t.to}:00</span></div></div>`;
        h += UI.sec('Прогноз');
        for (let i = 1; i <= 4; i++) {
          const ws = weatherOf(M.slot() + i), at = new Date((M.slot() + i) * SLOT);
          h += UI.card({ k: 'fc' + i, icon: ws.icon, title: `${U.pad(at.getHours())}:${U.pad(at.getMinutes())} — ${ws.name}`, desc: ws.desc });
        }
        h += UI.sec('Время суток');
        for (const x of TIMES) h += UI.card({ k: 'tm' + x.id, icon: x.icon, title: `${x.name} (${x.from}:00–${x.to}:00)`, desc: x.desc, cls: x === t ? 'sel' : '' });
        return h;
      },
    }],
  });

  // значок погоды в верхней панели
  UI.view('hudExtra', 1, () => G.has('weather') ? `<span class="cchip wx" data-k="wx">${M.now().icon}${M.time().icon}</span>` : '');

  // частицы погоды на фоне
  function renderFx() {
    const box = document.getElementById('weatherFx');
    if (!box) return;
    const w = G.has('weather') && G.S.settings.fx !== false ? M.now() : null;
    const fx = w && w.fx ? w.fx : '';
    if (fx === shownFx) return;
    shownFx = fx;
    let h = '';
    const n = { rain: 40, storm: 55, snow: 30, stars: 18, wind: 10, fog: 0 }[fx] || 0;
    for (let i = 0; i < n; i++) {
      const left = U.rand(0, 100).toFixed(1), dur = (fx === 'snow' ? U.rand(6, 11) : fx === 'stars' ? U.rand(2, 4) : fx === 'wind' ? U.rand(4, 8) : U.rand(0.5, 0.9)).toFixed(2), del = U.rand(0, 8).toFixed(2);
      const ch = fx === 'snow' ? '❄' : fx === 'stars' ? '✦' : fx === 'wind' ? '🍃' : '';
      h += `<i class="wp ${fx}" style="left:${left}%;animation-duration:${dur}s;animation-delay:-${del}s">${ch}</i>`;
    }
    if (fx === 'fog') h = '<i class="fog"></i><i class="fog f2"></i>';
    box.className = fx;
    box.innerHTML = h;
  }
  G.on('second', renderFx);
  G.on('boot', renderFx);
  // гроза: редкие вспышки молний
  G.on('second', () => { if (shownFx === 'storm' && Math.random() < 0.04 && UI.tab === 'tap') FX.flash('rgba(200,220,255,.35)'); });
  // звездопад: золотые объекты дают звёзды
  G.on('golden', () => { if (G.has('weather') && M.now().id === 'starfall') { G.add('stars', 1); G.toast('🌠 Звездопад: ⭐ +1'); } });
})();
