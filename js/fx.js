'use strict';
/* ============================================================
   fx.js — визуальные эффекты на canvas поверх игры:
   всплывающие цифры «+N», частицы, вспышки, тряска экрана.
   ============================================================ */
const FX = (() => {
  let cv, cx, W = 0, H = 0, dpr = 1, lastT = 0, wasEmpty = true;
  const parts = [];   // частицы
  const floats = [];  // всплывающие надписи
  const rings = [];   // расходящиеся кольца

  function resize() {
    dpr = Math.min(2, window.devicePixelRatio || 1);
    W = window.innerWidth; H = window.innerHeight;
    cv.width = Math.floor(W * dpr); cv.height = Math.floor(H * dpr);
    cv.style.width = W + 'px'; cv.style.height = H + 'px';
    cx.setTransform(dpr, 0, 0, dpr, 0, 0);
  }

  function init() {
    cv = document.getElementById('fx');
    cx = cv.getContext('2d');
    resize();
    window.addEventListener('resize', resize);
    requestAnimationFrame(loop);
  }

  const on = () => !G.S || G.S.settings.fx !== false;

  // всплывающая надпись
  function float(x, y, text, color = '#fff', size = 22, life = 0.9) {
    if (floats.length > 50) floats.shift();
    floats.push({ x: x + U.rand(-14, 14), y: y - 10, text, color, size, t: 0, life, vy: -110 - Math.random() * 30 });
  }
  // разлёт частиц
  function burst(x, y, n = 8, colors = ['#22e6ff', '#ff3df2', '#ffd34d'], speed = 220, size = 3) {
    if (!on()) return;
    n = Math.min(n, 400 - parts.length);
    for (let i = 0; i < n; i++) {
      const a = Math.random() * Math.PI * 2, sp = speed * (0.35 + Math.random() * 0.8);
      parts.push({
        x, y, vx: Math.cos(a) * sp, vy: Math.sin(a) * sp - 60,
        t: 0, life: 0.45 + Math.random() * 0.45, r: size * (0.6 + Math.random() * 0.9),
        c: colors[(Math.random() * colors.length) | 0],
      });
    }
  }
  function ring(x, y, color = '#22e6ff', maxR = 120) {
    if (!on()) return;
    if (rings.length > 12) rings.shift();
    rings.push({ x, y, color, t: 0, life: 0.5, maxR });
  }
  // тряска экрана
  function shake(strong) {
    if (!on()) return;
    const app = document.getElementById('app');
    app.classList.remove('shake', 'shake2');
    void app.offsetWidth;
    app.classList.add(strong ? 'shake2' : 'shake');
  }
  // вспышка цвета на весь экран
  function flash(color = 'rgba(255,255,255,.35)') {
    if (!on()) return;
    const f = document.getElementById('flash');
    f.style.background = color;
    f.classList.remove('go'); void f.offsetWidth; f.classList.add('go');
  }

  function loop(ts) {
    const dt = lastT ? Math.min(0.05, (ts - lastT) / 1000) : 0;
    lastT = ts;
    const empty = !parts.length && !floats.length && !rings.length;
    if (!empty || !wasEmpty) {
      cx.clearRect(0, 0, W, H);
      // частицы
      for (let i = parts.length - 1; i >= 0; i--) {
        const p = parts[i];
        p.t += dt;
        if (p.t >= p.life) { parts.splice(i, 1); continue; }
        p.vy += 520 * dt; p.vx *= 0.985;
        p.x += p.vx * dt; p.y += p.vy * dt;
        cx.globalAlpha = 1 - p.t / p.life;
        cx.fillStyle = p.c;
        cx.beginPath(); cx.arc(p.x, p.y, p.r, 0, 6.283); cx.fill();
      }
      // кольца
      for (let i = rings.length - 1; i >= 0; i--) {
        const r = rings[i];
        r.t += dt;
        if (r.t >= r.life) { rings.splice(i, 1); continue; }
        const k = r.t / r.life;
        cx.globalAlpha = (1 - k) * 0.8;
        cx.strokeStyle = r.color; cx.lineWidth = 3 * (1 - k) + 1;
        cx.beginPath(); cx.arc(r.x, r.y, 20 + k * r.maxR, 0, 6.283); cx.stroke();
      }
      // надписи
      cx.textAlign = 'center'; cx.textBaseline = 'middle';
      for (let i = floats.length - 1; i >= 0; i--) {
        const f = floats[i];
        f.t += dt;
        if (f.t >= f.life) { floats.splice(i, 1); continue; }
        const k = f.t / f.life;
        f.y += f.vy * dt; f.vy *= 0.96;
        const pop = k < 0.12 ? 0.6 + k / 0.12 * 0.5 : 1.1 - Math.min(0.1, (k - 0.12));
        cx.globalAlpha = k > 0.6 ? 1 - (k - 0.6) / 0.4 : 1;
        cx.font = `800 ${Math.round(f.size * pop)}px system-ui, -apple-system, Segoe UI, Roboto, sans-serif`;
        cx.lineWidth = 4; cx.strokeStyle = 'rgba(0,0,0,.65)';
        cx.strokeText(f.text, f.x, f.y);
        cx.fillStyle = f.color;
        cx.fillText(f.text, f.x, f.y);
      }
      cx.globalAlpha = 1;
    }
    wasEmpty = empty;
    requestAnimationFrame(loop);
  }

  return { init, float, burst, ring, shake, flash };
})();
